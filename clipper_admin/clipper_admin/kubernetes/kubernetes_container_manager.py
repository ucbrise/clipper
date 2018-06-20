from __future__ import absolute_import, division, print_function
from ..container_manager import (
    create_model_container_label, ContainerManager, CLIPPER_DOCKER_LABEL,
    CLIPPER_MODEL_CONTAINER_LABEL, CLIPPER_QUERY_FRONTEND_ID_LABEL,
    CLIPPER_INTERNAL_MANAGEMENT_PORT, CLIPPER_INTERNAL_QUERY_PORT,
    CLIPPER_INTERNAL_METRIC_PORT)
from ..exceptions import ClipperException
from .kubernetes_metric_utils import start_prometheus, CLIPPER_FRONTEND_EXPORTER_IMAGE

from contextlib import contextmanager
from kubernetes import client, config
from kubernetes.client.rest import ApiException
from kubernetes.client import configuration, V1DeleteOptions
import logging
import json
import yaml
import os
import time

CLIPPER_QUERY_FRONTEND_DEPLOYMENT_LABEL = "ai.clipper.name=query-frontend"

logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))


@contextmanager
def _pass_conflicts():
    try:
        yield
    except ApiException as e:
        body = json.loads(e.body)
        if body['reason'] == 'AlreadyExists':
            logger.info("{} already exists, skipping!".format(body['details']))
            pass
        else:
            raise e


class KubernetesContainerManager(ContainerManager):
    def __init__(self,
                 kubernetes_proxy_addr=None,
                 redis_ip=None,
                 redis_port=6379,
                 useInternalIP=False,
                 namespace='default',
                 create_namespace_if_not_exists=False):
        """

        Parameters
        ----------
        kubernetes_proxy_addr : str, optional
            The proxy address if you are proxying connections locally using ``kubectl proxy``.
            If this argument is provided, Clipper will construct the appropriate proxy
            URLs for accessing Clipper's Kubernetes services, rather than using the API server
            addres provided in your kube config.
        redis_ip : str, optional
            The address of a running Redis cluster. If set to None, Clipper will start
            a Redis deployment for you.
        redis_port : int, optional
            The Redis port. If ``redis_ip`` is set to None, Clipper will start Redis on this port.
            If ``redis_ip`` is provided, Clipper will connect to Redis on this port.
        useInternalIP : bool, optional
            Use Internal IP of the K8S nodes . If ``useInternalIP`` is set to False, Clipper will
            throw an exception if none of the nodes have ExternalDNS.
            If ``useInternalIP`` is set to true, Clipper will use the Internal IP of the K8S node
            if no ExternalDNS exists for any of the nodes.
        namespace: str, optional
            The Kubernetes namespace to use .
            If this argument is provided, all Clipper artifacts and resources will be created in this
            k8s namespace. If not "default" namespace is used.
        create_namespace_if_not_exists: bool, False
            Create a k8s namespace if the namespace doesnt already exist.
            If this argument is provided and the k8s namespace does not exist a new k8s namespace will
            be created.

        Note
        ----
        Clipper stores all persistent configuration state (such as registered application and model
        information) in Redis. If you want Clipper to be durable and able to recover from failures,
        we recommend configuring your own persistent and replicated Redis cluster rather than
        letting Clipper launch one for you.
        """

        if kubernetes_proxy_addr is not None:
            self.kubernetes_proxy_addr = kubernetes_proxy_addr
            self.use_k8s_proxy = True
        else:
            self.use_k8s_proxy = False

        self.redis_ip = redis_ip
        self.redis_port = redis_port
        self.useInternalIP = useInternalIP
        config.load_kube_config()
        configuration.assert_hostname = False
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()

        # Check if namespace exists and if create flag set ...create the namespace or throw error
        namespaces = []
        for ns in self._k8s_v1.list_namespace().items:
            namespaces.append(ns.metadata.name)

        if namespace in namespaces:
            self.k8s_namespace = namespace
        elif create_namespace_if_not_exists:
            body = client.V1Namespace()
            body.metadata = client.V1ObjectMeta(name=namespace)
            try:
                self._k8s_v1.create_namespace(body)
            except ApiException as e:
                logging.error(
                    "Exception creating Kubernetes namespace: {}".format(e))
                raise ClipperException(
                    "Could not create Kubernetes namespace. "
                    "Reason: {}".format(e.reason))
            self.k8s_namespace = namespace
        else:
            msg = "Error connecting to Kubernetes cluster. Namespace does not exist"
            logger.error(msg)
            raise ClipperException(msg)

    def start_clipper(self,
                      query_frontend_image,
                      mgmt_frontend_image,
                      cache_size,
                      num_frontend_replicas=1):
        self.num_frontend_replicas = num_frontend_replicas

        # If an existing Redis service isn't provided, start one
        if self.redis_ip is None:
            name = 'redis'
            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=yaml.load(
                        open(
                            os.path.join(cur_dir,
                                         '{}-deployment.yaml'.format(name)))),
                    namespace=self.k8s_namespace)

            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                body["spec"]["ports"][0]["port"] = self.redis_port
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace=self.k8s_namespace)
            time.sleep(10)

        for name, img in zip(['mgmt-frontend', 'query-frontend'],
                             [mgmt_frontend_image, query_frontend_image]):
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir,
                                     '{}-deployment.yaml'.format(name))))
                if self.redis_ip is not None:
                    args = [
                        "--redis_ip={}".format(self.redis_ip),
                        "--redis_port={}".format(self.redis_port)
                    ]
                    if name is 'query-frontend':
                        args.append(
                            "--prediction_cache_size={}".format(cache_size))
                    body["spec"]["template"]["spec"]["containers"][0][
                        "args"] = args
                body["spec"]["template"]["spec"]["containers"][0][
                    "image"] = img

                if name is 'query-frontend':
                    # Create multiple query frontend
                    for query_frontend_id in range(num_frontend_replicas):
                        # Create single query frontend depolyment
                        body['metadata']['name'] = 'query-frontend-{}'.format(
                            query_frontend_id)
                        body['spec']['template']['metadata']['labels'][
                            CLIPPER_QUERY_FRONTEND_ID_LABEL] = str(
                                query_frontend_id)

                        body['spec']['template']['spec']['containers'][1][
                            'image'] = CLIPPER_FRONTEND_EXPORTER_IMAGE

                        self._k8s_beta.create_namespaced_deployment(
                            body=body, namespace=self.k8s_namespace)

                        # Create single query frontend rpc service
                        # Don't confuse this with query frontend service, which
                        #   is created after the loop as a single user facing
                        #   service.
                        rpc_service_body = yaml.load(
                            open(
                                os.path.join(
                                    cur_dir,
                                    '{}-rpc-service.yaml'.format(name))))
                        rpc_service_body['metadata'][
                            'name'] = 'query-frontend-{}'.format(
                                query_frontend_id)
                        rpc_service_body['spec']['selector'][
                            CLIPPER_QUERY_FRONTEND_ID_LABEL] = str(
                                query_frontend_id)

                        self._k8s_v1.create_namespaced_service(
                            body=rpc_service_body,
                            namespace=self.k8s_namespace)

                else:
                    self._k8s_beta.create_namespaced_deployment(
                        body=body, namespace=self.k8s_namespace)

            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace=self.k8s_namespace)

        start_prometheus(self._k8s_v1, self._k8s_beta, self.k8s_namespace)

        self.connect()

    def connect(self):
        nodes = self._k8s_v1.list_node()

        external_node_hosts = []
        for node in nodes.items:
            for addr in node.status.addresses:
                if addr.type == "ExternalDNS":
                    external_node_hosts.append(addr.address)

        if len(external_node_hosts) == 0 and (self.useInternalIP):
            msg = "No external node addresses found. Using Internal IP address"
            logger.warn(msg)
            for addr in node.status.addresses:
                if addr.type == "InternalIP":
                    external_node_hosts.append(addr.address)

        if len(external_node_hosts) == 0:
            msg = "Error connecting to Kubernetes cluster. No external node addresses found"
            logger.error(msg)
            raise ClipperException(msg)

        self.external_node_hosts = external_node_hosts
        logger.info("Found {num_nodes} nodes: {nodes}".format(
            num_nodes=len(external_node_hosts),
            nodes=", ".join(external_node_hosts)))

        try:
            mgmt_frontend_ports = self._k8s_v1.read_namespaced_service(
                name="mgmt-frontend", namespace=self.k8s_namespace).spec.ports
            for p in mgmt_frontend_ports:
                if p.name == "1338":
                    self.clipper_management_port = p.node_port
                    logger.info("Setting Clipper mgmt port to {}".format(
                        self.clipper_management_port))

            query_frontend_ports = self._k8s_v1.read_namespaced_service(
                name="query-frontend", namespace=self.k8s_namespace).spec.ports
            for p in query_frontend_ports:
                if p.name == "1337":
                    self.clipper_query_port = p.node_port
                    logger.info("Setting Clipper query port to {}".format(
                        self.clipper_query_port))
                elif p.name == "7000":
                    self.clipper_rpc_port = p.node_port

            query_frontend_deployments = self._k8s_beta.list_namespaced_deployment(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_QUERY_FRONTEND_DEPLOYMENT_LABEL).items
            self.num_frontend_replicas = len(query_frontend_deployments)

            metrics_ports = self._k8s_v1.read_namespaced_service(
                name="metrics", namespace=self.k8s_namespace).spec.ports
            for p in metrics_ports:
                if p.name == "9090":
                    self.clipper_metric_port = p.node_port
                    logger.info("Setting Clipper metric port to {}".format(
                        self.clipper_metric_port))

        except ApiException as e:
            logging.warn(
                "Exception connecting to Clipper Kubernetes cluster: {}".
                format(e))
            raise ClipperException(
                "Could not connect to Clipper Kubernetes cluster. "
                "Reason: {}".format(e))

    def deploy_model(self, name, version, input_type, image, num_replicas=1):
        with _pass_conflicts():
            for query_frontend_id in range(self.num_frontend_replicas):
                deployment_name = get_model_deployment_name(
                    name, version, query_frontend_id)
                body = {
                    'apiVersion': 'extensions/v1beta1',
                    'kind': 'Deployment',
                    'metadata': {
                        "name": deployment_name,
                        "label": {
                            "test": "readiness"
                        },
                    },
                    'spec': {
                        'replicas': num_replicas,
                        'template': {
                            'metadata': {
                                'labels': {
                                    CLIPPER_MODEL_CONTAINER_LABEL:
                                    create_model_container_label(
                                        name, version),
                                    CLIPPER_DOCKER_LABEL:
                                    ""
                                },
                                'annotations': {
                                    "prometheus.io/scrape": "true",
                                    "prometheus.io/port": "1390",
                                    "test": "readiness",
                                }
                            },
                            'spec': {
                                'containers': [{
                                    'name':
                                    deployment_name,
                                    'image':
                                    image,
                                    'imagePullPolicy':
                                    'Always',
                                    'readinessProbe': {
                                        'exec': {
                                            'command':
                                            ['cat', '/model_is_ready.check']
                                        },
                                        'initialDelaySeconds': 3,
                                        'periodSeconds': 3
                                    },
                                    'ports': [{
                                        'containerPort': 80
                                    }, {
                                        'containerPort': 1390
                                    }],
                                    'env': [{
                                        'name': 'CLIPPER_MODEL_NAME',
                                        'value': name
                                    }, {
                                        'name': 'CLIPPER_MODEL_VERSION',
                                        'value': str(version)
                                    }, {
                                        'name':
                                        'CLIPPER_IP',
                                        'value':
                                        'query-frontend-{}'.format(
                                            query_frontend_id)
                                    }, {
                                        'name': 'CLIPPER_INPUT_TYPE',
                                        'value': input_type
                                    }]
                                }]
                            }
                        }
                    }
                }
                self._k8s_beta.create_namespaced_deployment(
                    body=body, namespace=self.k8s_namespace)

                while self._k8s_beta.read_namespaced_deployment_status(
                    name=deployment_name, namespace=self.k8s_namespace).status.available_replicas \
                        != num_replicas:
                    time.sleep(3)

    def get_num_replicas(self, name, version):
        deployment_name = get_model_deployment_name(
            name, version, query_frontend_id=0)
        response = self._k8s_beta.read_namespaced_deployment_scale(
            name=deployment_name, namespace=self.k8s_namespace)

        return response.spec.replicas

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        # NOTE: assumes `metadata.name` can identify the model deployment.
        for query_frontend_id in range(self.num_frontend_replicas):
            deployment_name = get_model_deployment_name(
                name, version, query_frontend_id)

            self._k8s_beta.patch_namespaced_deployment_scale(
                name=deployment_name,
                namespace=self.k8s_namespace,
                body={
                    'spec': {
                        'replicas': num_replicas,
                    }
                })

            while self._k8s_beta.read_namespaced_deployment_status(
                name=deployment_name, namespace=self.k8s_namespace).status.available_replicas \
                    != num_replicas:
                time.sleep(3)

    def get_logs(self, logging_dir):
        logging_dir = os.path.abspath(os.path.expanduser(logging_dir))

        log_files = []
        if not os.path.exists(logging_dir):
            os.makedirs(logging_dir)
            logger.info("Created logging directory: %s" % logging_dir)

        for pod in self._k8s_v1.list_namespaced_pod(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL).items:
            for i, c in enumerate(pod.status.container_statuses):
                # log_file_name = "image_{image}:container_{id}.log".format(
                #     image=c.image_id, id=c.container_id)
                log_file_name = "{pod}_{num}.log".format(
                    pod=pod.metadata.name, num=str(i))
                log_file_alt = "{cname}.log".format(cname=c.name)
                logger.info("log file name: {}".format(log_file_name))
                logger.info("log alt file name: {}".format(log_file_alt))
                log_file = os.path.join(logging_dir, log_file_name)
                with open(log_file, "w") as lf:
                    lf.write(
                        self._k8s_v1.read_namespaced_pod_log(
                            namespace=self.k8s_namespace,
                            name=pod.metadata.name,
                            container=c.name))
                log_files.append(log_file)
        return log_files

    def stop_models(self, models):
        # Stops all deployments of pods running Clipper models with the specified
        # names and versions.
        try:
            for m in models:
                for v in models[m]:
                    self._k8s_beta.delete_collection_namespaced_deployment(
                        namespace=self.k8s_namespace,
                        label_selector="{label}:{val}".format(
                            label=CLIPPER_MODEL_CONTAINER_LABEL,
                            val=create_model_container_label(m, v)))
        except ApiException as e:
            logger.warn(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all_model_containers(self):
        try:
            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logger.warn(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all(self):
        logger.info("Stopping all running Clipper resources")

        try:
            for service in self._k8s_v1.list_namespaced_service(
                    namespace=self.k8s_namespace,
                    label_selector=CLIPPER_DOCKER_LABEL).items:
                service_name = service.metadata.name
                self._k8s_v1.delete_namespaced_service(
                    namespace=self.k8s_namespace,
                    name=service_name,
                    body=V1DeleteOptions())

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_beta.delete_collection_namespaced_replica_set(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_replication_controller(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)

            self._k8s_v1.delete_collection_namespaced_config_map(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL)
        except ApiException as e:
            logging.warn(
                "Exception deleting kubernetes resources: {}".format(e))

    def get_registry(self):
        return self.registry

    def get_admin_addr(self):
        if self.use_k8s_proxy:
            return ("{proxy_addr}/api/v1/namespaces/{ns}/"
                    "services/mgmt-frontend:{port}/proxy").format(
                        proxy_addr=self.kubernetes_proxy_addr,
                        ns=self.k8s_namespace,
                        port=CLIPPER_INTERNAL_MANAGEMENT_PORT)

        else:
            return "{host}:{port}".format(
                host=self.external_node_hosts[0],
                port=self.clipper_management_port)

    def get_query_addr(self):
        if self.use_k8s_proxy:
            return ("{proxy_addr}/api/v1/namespaces/{ns}/"
                    "services/query-frontend:{port}/proxy").format(
                        proxy_addr=self.kubernetes_proxy_addr,
                        ns=self.k8s_namespace,
                        port=CLIPPER_INTERNAL_QUERY_PORT)
        else:
            return "{host}:{port}".format(
                host=self.external_node_hosts[0], port=self.clipper_query_port)

    def get_metric_addr(self):
        if self.use_k8s_proxy:
            return ("{proxy_addr}/api/v1/namespaces/{ns}/"
                    "services/metrics:{port}/proxy").format(
                        proxy_addr=self.kubernetes_proxy_addr,
                        ns=self.k8s_namespace,
                        port=CLIPPER_INTERNAL_METRIC_PORT)
        else:
            return "{host}:{port}".format(
                host=self.external_node_hosts[0],
                port=self.clipper_metric_port)


def get_model_deployment_name(name, version, query_frontend_id):
    return "{name}-{version}-deployment-at-{query_frontend_id}".format(
        name=name, version=version, query_frontend_id=query_frontend_id)
