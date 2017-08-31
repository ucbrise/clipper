from __future__ import absolute_import, division, print_function
from ..container_manager import (create_model_container_label,
                                 ContainerManager, CLIPPER_DOCKER_LABEL,
                                 CLIPPER_MODEL_CONTAINER_LABEL)
from ..exceptions import ClipperException

from contextlib import contextmanager
from kubernetes import client, config
from kubernetes.client.rest import ApiException
from kubernetes.client import configuration
import logging
import json
import yaml
import os
import time

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
    def __init__(self, kubernetes_api_ip, redis_ip=None, redis_port=6379):
        """
        Parameters
        ----------
        kubernetes_api_ip : str
            The hostname or IP address of the Kubernetes API server for your Kubernetes cluster.
        redis_ip : str, optional
            The address of a running Redis cluster. If set to None, Clipper will start
            a Redis deployment for you.
        redis_port : int, optional
            The Redis port. If ``redis_ip`` is set to None, Clipper will start Redis on this port.
            If ``redis_ip`` is provided, Clipper will connect to Redis on this port.

        Note
        ----
        Clipper stores all persistent configuration state (such as registered application and model
        information) in Redis. If you want Clipper to be durable and able to recover from failures,
        we recommend configuring your own persistent and replicated Redis cluster rather than letting
        Clipper launch one for you.
        """

        self.kubernetes_api_ip = kubernetes_api_ip
        self.redis_ip = redis_ip
        self.redis_port = redis_port

        config.load_kube_config()
        configuration.assert_hostname = False
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()

    def start_clipper(self, query_frontend_image, mgmt_frontend_image,
                      cache_size):
        # If an existing Redis service isn't provided, start one
        if self.redis_ip is None:
            name = 'redis'
            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=yaml.load(
                        open(
                            os.path.join(cur_dir, '{}-deployment.yaml'.format(
                                name)))),
                    namespace='default')
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                body["spec"]["ports"][0]["port"] = self.redis_port
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace='default')
            time.sleep(10)
        for name, img in zip(['mgmt-frontend', 'query-frontend'],
                             [mgmt_frontend_image, query_frontend_image]):
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-deployment.yaml'.format(
                            name))))
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
                self._k8s_beta.create_namespaced_deployment(
                    body=body, namespace='default')
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace='default')
        self.connect()

    def connect(self):
        nodes = self._k8s_v1.list_node()
        external_node_hosts = []
        for node in nodes.items:
            for addr in node.status.addresses:
                if addr.type == "ExternalDNS":
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
                name="mgmt-frontend", namespace='default').spec.ports
            for p in mgmt_frontend_ports:
                if p.name == "1338":
                    self.clipper_management_port = p.node_port
                    logger.info("Setting Clipper mgmt port to {}".format(
                        self.clipper_management_port))
            query_frontend_ports = self._k8s_v1.read_namespaced_service(
                name="query-frontend", namespace='default').spec.ports
            for p in query_frontend_ports:
                if p.name == "1337":
                    self.clipper_query_port = p.node_port
                    logger.info("Setting Clipper query port to {}".format(
                        self.clipper_query_port))
                elif p.name == "7000":
                    self.clipper_rpc_port = p.node_port
        except ApiException as e:
            logging.warn(
                "Exception connecting to Clipper Kubernetes cluster: {}".
                format(e))
            raise ClipperException(
                "Could not connect to Clipper Kubernetes cluster. "
                "Reason: {}".format(e))

    def deploy_model(self, name, version, input_type, image, num_replicas=1):
        with _pass_conflicts():
            deployment_name = get_model_deployment_name(name, version)
            body = {
                'apiVersion': 'extensions/v1beta1',
                'kind': 'Deployment',
                'metadata': {
                    "name": deployment_name
                },
                'spec': {
                    'replicas': num_replicas,
                    'template': {
                        'metadata': {
                            'labels': {
                                CLIPPER_MODEL_CONTAINER_LABEL:
                                create_model_container_label(name, version),
                                CLIPPER_DOCKER_LABEL:
                                ""
                            }
                        },
                        'spec': {
                            'containers': [{
                                'name':
                                deployment_name,
                                'image':
                                image,
                                'ports': [{
                                    'containerPort': 80
                                }],
                                'env': [{
                                    'name': 'CLIPPER_MODEL_NAME',
                                    'value': name
                                }, {
                                    'name': 'CLIPPER_MODEL_VERSION',
                                    'value': str(version)
                                }, {
                                    'name': 'CLIPPER_IP',
                                    'value': 'query-frontend'
                                }]
                            }]
                        }
                    }
                }
            }
            self._k8s_beta.create_namespaced_deployment(
                body=body, namespace='default')

    def get_num_replicas(self, name, version):
        deployment_name = get_model_deployment_name(name, version)
        response = self._k8s_beta.read_namespaced_deployments_scale(
            name=deployment_name, namespace='default')

        return response.spec.replicas

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        # NOTE: assumes `metadata.name` can identify the model deployment.
        deployment_name = get_model_deployment_name(name, version)

        self._k8s_beta.patch_namespaced_deployments_scale(
            name=deployment_name,
            namespace='default',
            body={'spec': {
                'replicas': num_replicas,
            }})

    def get_logs(self, logging_dir):
        logging_dir = os.path.abspath(os.path.expanduser(logging_dir))

        log_files = []
        if not os.path.exists(logging_dir):
            os.makedirs(logging_dir)
            logger.info("Created logging directory: %s" % logging_dir)

        for pod in self._k8s_v1.list_namespaced_pod(
                namespace='default',
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
                            namespace='default',
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
                        namespace='default',
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
                namespace='default',
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logger.warn(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all(self):
        logger.info("Stopping all running Clipper resources")

        try:
            for service in self._k8s_v1.list_namespaced_service(
                    namespace='default',
                    label_selector=CLIPPER_DOCKER_LABEL).items:
                service_name = service.metadata.name
                self._k8s_v1.delete_namespaced_service(
                    namespace='default', name=service_name)

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_beta.delete_collection_namespaced_replica_set(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_replication_controller(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default',
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logging.warn(
                "Exception deleting kubernetes resources: {}".format(e))

    def get_registry(self):
        return self.registry

    def get_admin_addr(self):
        return "{host}:{port}".format(
            host=self.external_node_hosts[0],
            port=self.clipper_management_port)

    def get_query_addr(self):
        return "{host}:{port}".format(
            host=self.external_node_hosts[0], port=self.clipper_query_port)


def get_model_deployment_name(name, version):
    return "{name}-{version}-deployment".format(name=name, version=version)
