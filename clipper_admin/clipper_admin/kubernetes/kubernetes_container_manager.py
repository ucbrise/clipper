from __future__ import absolute_import, division, print_function
from ..container_manager import (
    create_model_container_label, ContainerManager, CLIPPER_DOCKER_LABEL,
    CLIPPER_MODEL_CONTAINER_LABEL, CLIPPER_INTERNAL_RPC_PORT,
    CLIPPER_INTERNAL_MANAGEMENT_PORT, CLIPPER_INTERNAL_QUERY_PORT,
    CLIPPER_INTERNAL_METRIC_PORT, CLIPPER_NAME_LABEL, ClusterAdapter)
from ..exceptions import ClipperException
from .kubernetes_metric_utils import PROM_VERSION

from contextlib import contextmanager
from kubernetes import client, config
from kubernetes.client.rest import ApiException
from kubernetes.client import configuration, V1DeleteOptions
import logging
import json
import yaml
import os
import time
import jinja2
from jinja2.exceptions import TemplateNotFound

logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))

CLUSTER_IP = 'ClusterIP'
NODE_PORT = 'NodePort'
LOAD_BALANCER = 'LoadBalancer'
EXTERNAL_NAME = 'ExternalName'
OFFICIAL_K8S_SERVICE_TYPE = [CLUSTER_IP, NODE_PORT, LOAD_BALANCER, EXTERNAL_NAME]

DEFAULT_CLIPPER_SERVICE_TYPES = {
    'redis': NODE_PORT,
    'management': NODE_PORT,
    'query': NODE_PORT,
    'query-rpc': NODE_PORT,
    'metric': NODE_PORT
}

DUMMY_CLUSTER_NAME = 'cluster-name'

CONFIG_FILES = {
    'k8s': {
        'service_types': '{cluster_name}-k8s-service-types.yaml'.format(
            cluster_name=DUMMY_CLUSTER_NAME)
    },
    'redis': {
        'service': 'redis-service.yaml',
        'deployment': 'redis-deployment.yaml'
    },
    'management': {
        'service': 'mgmt-frontend-service.yaml',
        'deployment': 'mgmt-frontend-deployment.yaml'
    },
    'query': {
        'service': {
            'query': 'query-frontend-service.yaml',
            'rpc': 'query-frontend-rpc-service.yaml'
        },
        'deployment': 'query-frontend-deployment.yaml',
    },
    'metric': {
        'service': 'prom_service.yaml',
        'deployment': 'prom_deployment.yaml',
        'config': 'prom_configmap.yaml'
    },
    'rbac': {
        'clusterrole': 'rbac_cluster_role.yaml',
        'clusterrolebinding': 'rbac_cluster_role_binding.yaml',
    },
    'model': {
        'deployment': 'model-container-template.yaml'
    }
}


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
                 cluster_name="default-cluster",
                 kubernetes_proxy_addr=None,
                 redis_ip=None,
                 redis_port=6379,
                 useInternalIP=False,
                 namespace='default',
                 service_types=None,
                 create_namespace_if_not_exists=False):
        """

        Parameters
        ----------
        cluster_name : str
            A unique name for this Clipper cluster. This can be used to run multiple Clipper
            clusters on the same Kubernetes cluster without interfering with each other.
            Kubernetes cluster name must follow Kubernetes label value naming rule, namely:
            Valid label values must be 63 characters or less and must be empty or begin and end with
            an alphanumeric character ([a-z0-9A-Z]) with dashes (-), underscores (_), dots (.),
            and alphanumerics between. See more at:
            https://kubernetes.io/docs/concepts/overview/working-with-objects/labels/#syntax-and-character-set
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
        service_types: dict, optional
            Specify what kind of Kubernetes service you want.
            You must use predefined 'ServiceTypes' in Kubernetes as value. See more at:
            https://kubernetes.io/docs/concepts/services-networking/service/#publishing-services-service-types
            For example, service_types = {
                'redis': 'NodePort',
                'management': 'LoadBalancer',
                'query': 'LoadBalancer',
                'query-rpc': 'ClusterIP',
                'metric': 'LoadBalancer'
            }
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

        self.cluster_name = cluster_name

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
        self._k8s_rbac = client.RbacAuthorizationV1beta1Api()


        # Create the template engine
        # Config: Any variable missing -> Error
        self.template_engine = jinja2.Environment(
            loader=jinja2.FileSystemLoader(cur_dir, followlinks=True),
            undefined=jinja2.StrictUndefined)

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
            msg = "Error connecting to Kubernetes cluster. Namespace does not exist. You can pass in KubernetesContainerManager(create_namespace_if_not_exists=True) to crate this namespcae"
            logger.error(msg)
            raise ClipperException(msg)

        # Initialize logger with cluster identifier
        if self.k8s_namespace != "default":
            self.cluster_identifier = "{cluster}".format(
                cluster=self.cluster_name)
        else:
            self.cluster_identifier = "{ns}-{cluster}".format(
                ns=self.k8s_namespace, cluster=self.cluster_name)

        self.logger = ClusterAdapter(logger, {
            'cluster_name': self.cluster_identifier
        })

        self.service_types = self._determine_service_types(service_types)

    def _determine_service_types(self, st):
        yaml_file_name = CONFIG_FILES['k8s']['service_types'].replace(
            DUMMY_CLUSTER_NAME, self.cluster_identifier)
        res = DEFAULT_CLIPPER_SERVICE_TYPES

        if st is None:
            try:
                res = self._generate_config(yaml_file_name)
            except TemplateNotFound:
                res = DEFAULT_CLIPPER_SERVICE_TYPES

        else:
            if not isinstance(st, dict):
                raise ClipperException(
                    "service_types must be 'dict' type: {}".format(st))
            if set(st.keys()) != set(DEFAULT_CLIPPER_SERVICE_TYPES.keys()):
                raise ClipperException(
                    "service_types has unknown keys: {}".format(st.keys()))
            if any(v not in OFFICIAL_K8S_SERVICE_TYPE for v in set(st.values())):
                raise ClipperException(
                    "service_types has unknown values: {}".format(st.values()))
            if EXTERNAL_NAME in st.values():
                raise ClipperException(
                    "Clipper does not support '{}' service".format(EXTERNAL_NAME))
            res.update(st)

            with open(os.path.join(cur_dir, yaml_file_name), 'w') as f:
                yaml.dump(res, f)

        logging.info("Your service_types are {}".format(res))
        return res

    def start_clipper(self,
                      query_frontend_image,
                      mgmt_frontend_image,
                      frontend_exporter_image,
                      cache_size,
                      qf_http_thread_pool_size,
                      qf_http_timeout_request,
                      qf_http_timeout_content,
                      num_frontend_replicas=1):
        self._config_rbac()
        self._start_redis()
        self._start_mgmt(mgmt_frontend_image)
        self.num_frontend_replicas = num_frontend_replicas
        self._start_query(query_frontend_image, frontend_exporter_image,
                          cache_size, qf_http_thread_pool_size,
                          qf_http_timeout_request, qf_http_timeout_content,
                          num_frontend_replicas)
        self._start_prometheus()
        self.connect()

    def _start_redis(self, sleep_time=5):
        # If an existing Redis service isn't provided, start one
        if self.redis_ip is None:
            deployment_name = 'redis-at-{cluster_name}'.format(
                cluster_name=self.cluster_name)

            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=self._generate_config(
                        CONFIG_FILES['redis']['deployment'],
                        deployment_name=deployment_name,
                        cluster_name=self.cluster_name),
                    namespace=self.k8s_namespace)

            with _pass_conflicts():
                body = self._generate_config(
                    CONFIG_FILES['redis']['service'],
                    deployment_name=deployment_name,
                    public_redis_port=self.redis_port,
                    cluster_name=self.cluster_name,
                    service_type=self.service_types['redis'],
                )
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace=self.k8s_namespace)
            time.sleep(sleep_time)

            # Wait for max 10 minutes
            wait_count = 0
            while self._k8s_beta.read_namespaced_deployment(
                    name=deployment_name,
                    namespace=self.k8s_namespace).status.available_replicas != 1:
                time.sleep(3)
                wait_count += 3
                if wait_count > 600:
                    raise ClipperException(
                        "Could not create a Kubernetes deployment: {}".format(deployment_name))

            self.redis_ip = deployment_name

    def _start_mgmt(self, mgmt_image):
        with _pass_conflicts():
            mgmt_deployment_data = self._generate_config(
                CONFIG_FILES['management']['deployment'],
                image=mgmt_image,
                redis_service_host=self.redis_ip,
                redis_service_port=self.redis_port,
                cluster_name=self.cluster_name)
            self._k8s_beta.create_namespaced_deployment(
                body=mgmt_deployment_data, namespace=self.k8s_namespace)

        with _pass_conflicts():
            mgmt_service_data = self._generate_config(
                CONFIG_FILES['management']['service'],
                cluster_name=self.cluster_name,
                service_type=self.service_types['management'])
            self._k8s_v1.create_namespaced_service(
                body=mgmt_service_data, namespace=self.k8s_namespace)

    def _start_query(self, query_image, frontend_exporter_image, cache_size,
                     qf_http_thread_pool_size, qf_http_timeout_request,
                     qf_http_timeout_content, num_replicas):
        for query_frontend_id in range(num_replicas):
            with _pass_conflicts():
                query_deployment_data = self._generate_config(
                    CONFIG_FILES['query']['deployment'],
                    image=query_image,
                    exporter_image=frontend_exporter_image,
                    redis_service_host=self.redis_ip,
                    redis_service_port=self.redis_port,
                    cache_size=cache_size,
                    thread_pool_size=qf_http_thread_pool_size,
                    timeout_request=qf_http_timeout_request,
                    timeout_content=qf_http_timeout_content,
                    name='query-frontend-{}'.format(query_frontend_id),
                    id_label=str(query_frontend_id),
                    cluster_name=self.cluster_name)
                self._k8s_beta.create_namespaced_deployment(
                    body=query_deployment_data, namespace=self.k8s_namespace)

            with _pass_conflicts():
                query_rpc_service_data = self._generate_config(
                    CONFIG_FILES['query']['service']['rpc'],
                    name='query-frontend-{}'.format(query_frontend_id),
                    id_label=str(query_frontend_id),
                    cluster_name=self.cluster_name,
                    service_type=self.service_types['query-rpc'],
                )
                self._k8s_v1.create_namespaced_service(
                    body=query_rpc_service_data, namespace=self.k8s_namespace)

        with _pass_conflicts():
            query_frontend_service_data = self._generate_config(
                CONFIG_FILES['query']['service']['query'],
                cluster_name=self.cluster_name,
                service_type=self.service_types['query'])
            self._k8s_v1.create_namespaced_service(
                body=query_frontend_service_data, namespace=self.k8s_namespace)

    def _start_prometheus(self):
        with _pass_conflicts():
            configmap_data = self._generate_config(
                CONFIG_FILES['metric']['config'],
                cluster_name=self.cluster_name)
            self._k8s_v1.create_namespaced_config_map(
                body=configmap_data, namespace=self.k8s_namespace)

        with _pass_conflicts():
            deployment_data = self._generate_config(
                CONFIG_FILES['metric']['deployment'],
                version=PROM_VERSION,
                cluster_name=self.cluster_name,
            )
            self._k8s_beta.create_namespaced_deployment(
                body=deployment_data, namespace=self.k8s_namespace)

        with _pass_conflicts():
            service_data = self._generate_config(
                CONFIG_FILES['metric']['service'],
                cluster_name=self.cluster_name,
                service_type=self.service_types['metric'],
            )
            self._k8s_v1.create_namespaced_service(
                body=service_data, namespace=self.k8s_namespace)

    def _config_rbac(self):
        with _pass_conflicts():
            clusterrole_data = self._generate_config(
                CONFIG_FILES['rbac']['clusterrole'],
                cluster_name=self.cluster_name, namespace=self.k8s_namespace)
            self._k8s_rbac.create_cluster_role(
                body=clusterrole_data)

        with _pass_conflicts():
            clusterrolebinding_data = self._generate_config(
                CONFIG_FILES['rbac']['clusterrolebinding'],
                cluster_name=self.cluster_name, namespace=self.k8s_namespace)
            self._k8s_rbac.create_cluster_role_binding(
                body=clusterrolebinding_data)

    def _generate_config(self, file_path, **kwargs):
        template = self.template_engine.get_template(file_path)
        rendered = template.render(**kwargs)
        parsed = yaml.load(rendered, Loader=yaml.FullLoader)
        return parsed

    def connect(self):
        if any(v in [CLUSTER_IP, NODE_PORT] for v in self.service_types.values()):
            nodes = self._k8s_v1.list_node()

            external_node_hosts = []
            for node in nodes.items:
                for addr in node.status.addresses:
                    if addr.type == "ExternalDNS":
                        external_node_hosts.append(addr.address)

            if len(external_node_hosts) == 0 and self.useInternalIP:
                msg = "No external node addresses found. Using Internal IP address"
                self.logger.warning(msg)
                for node in nodes.items:
                    for addr in node.status.addresses:
                        if addr.type == "InternalIP":
                            external_node_hosts.append(addr.address)

            if len(external_node_hosts) == 0:
                msg = "Error connecting to Kubernetes cluster. No external node addresses found. You may pass in KubernetesContainerManager(useInternalIP=True) to connect to local Kubernetes cluster"
                self.logger.error(msg)
                raise ClipperException(msg)

            self.external_node_hosts = external_node_hosts
            self.logger.info("Found {num_nodes} nodes: {nodes}".format(
                num_nodes=len(external_node_hosts),
                nodes=", ".join(external_node_hosts)))

        try:
            v1service = self._k8s_v1.read_namespaced_service(
                name="mgmt-frontend-at-{cluster_name}".format(
                    cluster_name=self.cluster_name),
                namespace=self.k8s_namespace)
            mgmt_frontend_ports = v1service.spec.ports
            for p in mgmt_frontend_ports:
                if int(p.name) == CLIPPER_INTERNAL_MANAGEMENT_PORT:
                    self.clipper_management_port = p.node_port

            if self.service_types['management'] in [CLUSTER_IP, NODE_PORT]:
                self.logger.info("Setting Clipper mgmt port to {port}".format(
                    port=self.clipper_management_port))
            elif self.service_types['management'] == LOAD_BALANCER:
                self.clipper_management_ip = v1service.status.load_balancer.ingress[0].ip
                self.logger.info("Setting Clipper mgmt port to {ip}:{port}"
                                 .format(ip=self.clipper_management_ip,
                                         port=self.clipper_management_port))
            else:
                msg = "Unknown service_type of management: {}".format(
                        self.service_types['management'])
                raise ClipperException(msg)

            v1service = self._k8s_v1.read_namespaced_service(
                name="query-frontend-at-{cluster_name}".format(
                    cluster_name=self.cluster_name),
                namespace=self.k8s_namespace)
            query_frontend_ports = v1service.spec.ports
            for p in query_frontend_ports:
                if int(p.name) == CLIPPER_INTERNAL_QUERY_PORT:
                    self.clipper_query_port = p.node_port
                elif int(p.name) == CLIPPER_INTERNAL_RPC_PORT:
                    self.clipper_rpc_port = p.node_port

            if self.service_types['query'] in [CLUSTER_IP, NODE_PORT]:
                self.logger.info("Setting Clipper query port to {}".format(
                    self.clipper_query_port))
            elif self.service_types['query'] == LOAD_BALANCER:
                self.clipper_query_ip = v1service.status.load_balancer.ingress[0].ip
                self.logger.info("Setting Clipper query port to {ip}:{port}"
                                 .format(ip=self.clipper_query_ip,
                                         port=self.clipper_query_port))
            else:
                msg = "Unknown service_type of query: {}".format(
                        self.service_types['query'])
                raise ClipperException(msg)

            query_frontend_deployments = self._k8s_beta.list_namespaced_deployment(
                namespace=self.k8s_namespace,
                label_selector=
                "{name_label}=query-frontend, {cluster_label}={cluster_name}".
                format(
                    name_label=CLIPPER_NAME_LABEL,
                    cluster_label=CLIPPER_DOCKER_LABEL,
                    cluster_name=self.cluster_name)).items
            self.num_frontend_replicas = len(query_frontend_deployments)

            v1service = self._k8s_v1.read_namespaced_service(
                name="metrics-at-{cluster_name}".format(
                    cluster_name=self.cluster_name),
                namespace=self.k8s_namespace)
            metrics_ports = v1service.spec.ports
            for p in metrics_ports:
                if p.name == "9090":
                    self.clipper_metric_port = p.node_port

            if self.service_types['metric'] in [CLUSTER_IP, NODE_PORT]:
                self.logger.info("Setting Clipper metric port to {port}".format(
                    port=self.clipper_metric_port))
            elif self.service_types['metric'] == LOAD_BALANCER:
                self.clipper_metric_ip = v1service.status.load_balancer.ingress[0].ip
                self.logger.info("Setting Clipper metric port to {ip}:{port}"
                                 .format(ip=self.clipper_metric_ip,
                                         port=self.clipper_metric_port))
            else:
                msg = "Unknown service_type of metric: {}".format(
                    self.service_types['metric'])
                raise ClipperException(msg)

        except ApiException as e:
            logging.warning(
                "Exception connecting to Clipper Kubernetes cluster: {}".
                format(e))
            raise ClipperException(
                "Could not connect to Clipper Kubernetes cluster. "
                "Reason: {}".format(e))

    def deploy_model(self, name, version, input_type, image, num_replicas=1):
        for query_frontend_id in range(self.num_frontend_replicas):
            deployment_name = get_model_deployment_name(
                name, version, query_frontend_id, self.cluster_name)

            generated_body = self._generate_config(
                CONFIG_FILES['model']['deployment'],
                deployment_name=deployment_name,
                num_replicas=num_replicas,
                container_label=create_model_container_label(name, version),
                model_name=name,
                version=version,
                query_frontend_id=query_frontend_id,
                input_type=input_type,
                image=image,
                cluster_name=self.cluster_name)

            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=generated_body, namespace=self.k8s_namespace)

            # Wait for max 10 minutes
            wait_count = 0
            while self._k8s_beta.read_namespaced_deployment(
                name=deployment_name, namespace=self.k8s_namespace).status.available_replicas \
                    != num_replicas:
                time.sleep(3)
                wait_count += 3
                if wait_count > 600:
                    raise ClipperException(
                        "Could not create a Kubernetes deployment. "
                        "Model: {}-{} Image: {}".format(name, version, image))

    def get_num_replicas(self, name, version):
        deployment_name = get_model_deployment_name(
            name, version, query_frontend_id=0, cluster_name=self.cluster_name)
        response = self._k8s_beta.read_namespaced_deployment_scale(
            name=deployment_name, namespace=self.k8s_namespace)

        return response.spec.replicas

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        # NOTE: assumes `metadata.name` can identify the model deployment.
        for query_frontend_id in range(self.num_frontend_replicas):
            deployment_name = get_model_deployment_name(
                name, version, query_frontend_id, self.cluster_name)

            self._k8s_beta.patch_namespaced_deployment_scale(
                name=deployment_name,
                namespace=self.k8s_namespace,
                body={
                    'spec': {
                        'replicas': num_replicas,
                    }
                })

            # Wait for max 10 minutes
            wait_count = 0
            while self._k8s_beta.read_namespaced_deployment(
                name=deployment_name, namespace=self.k8s_namespace).status.available_replicas \
                    != num_replicas:
                time.sleep(3)
                wait_count += 3
                if wait_count > 600:
                    raise ClipperException(
                        "Could not update scale of the specified Deployment. "
                        "Model: {}-{} Image: {}".format(name, version, image))

    def get_logs(self, logging_dir):
        logging_dir = os.path.abspath(os.path.expanduser(logging_dir))

        log_files = []
        if not os.path.exists(logging_dir):
            os.makedirs(logging_dir)
            self.logger.info("Created logging directory: %s" % logging_dir)

        for pod in self._k8s_v1.list_namespaced_pod(
                namespace=self.k8s_namespace,
                label_selector=CLIPPER_DOCKER_LABEL).items:
            for i, c in enumerate(pod.status.container_statuses):
                log_file_name = "{pod}_{num}.log".format(
                    pod=pod.metadata.name, num=str(i))
                self.logger.info("log file name: {}".format(log_file_name))
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
                    label_selector="{label}={val}, {cluster_label}={cluster_name}".format(
                        label=CLIPPER_MODEL_CONTAINER_LABEL,
                        val=create_model_container_label(m, v),
                        cluster_label=CLIPPER_DOCKER_LABEL,
                        cluster_name=self.cluster_name)

                    self._k8s_beta.delete_collection_namespaced_deployment(
                        namespace=self.k8s_namespace,
                        label_selector=label_selector)

                    self._k8s_beta.delete_collection_namespaced_replica_set(
                        namespace=self.k8s_namespace,
                        label_selector=label_selector)

                    self._k8s_v1.delete_collection_namespaced_pod(
                        namespace=self.k8s_namespace,
                        label_selector=label_selector)

        except ApiException as e:
            self.logger.warning(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all_model_containers(self):
        try:
            label_selector="{label}, {cluster_label}={cluster_name}".format(
                label=CLIPPER_MODEL_CONTAINER_LABEL,
                cluster_label=CLIPPER_DOCKER_LABEL,
                cluster_name=self.cluster_name)

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace=self.k8s_namespace,
                label_selector=label_selector)

            self._k8s_beta.delete_collection_namespaced_replica_set(
                namespace=self.k8s_namespace,
                label_selector=label_selector)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace=self.k8s_namespace,
                label_selector=label_selector)

        except ApiException as e:
            self.logger.warning(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all(self, graceful=True):
        self.logger.info("Stopping all running Clipper resources")

        cluster_selector = "{cluster_label}={cluster_name}".format(
            cluster_label=CLIPPER_DOCKER_LABEL, cluster_name=self.cluster_name)

        try:
            for service in self._k8s_v1.list_namespaced_service(
                    namespace=self.k8s_namespace,
                    label_selector=cluster_selector).items:
                service_name = service.metadata.name
                self._k8s_v1.delete_namespaced_service(
                    namespace=self.k8s_namespace,
                    name=service_name,
                    body=V1DeleteOptions())

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace=self.k8s_namespace, label_selector=cluster_selector)

            self._k8s_beta.delete_collection_namespaced_replica_set(
                namespace=self.k8s_namespace, label_selector=cluster_selector)

            self._k8s_v1.delete_collection_namespaced_replication_controller(
                namespace=self.k8s_namespace, label_selector=cluster_selector)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace=self.k8s_namespace, label_selector=cluster_selector)

            self._k8s_v1.delete_collection_namespaced_config_map(
                namespace=self.k8s_namespace, label_selector=cluster_selector)

            self._k8s_rbac.delete_collection_cluster_role(
                label_selector=cluster_selector)

            self._k8s_rbac.delete_collection_cluster_role_binding(
                label_selector=cluster_selector)
        except ApiException as e:
            logging.warning(
                "Exception deleting kubernetes resources: {}".format(e))

        try:
            yaml_file_name = CONFIG_FILES['k8s']['service_types'].replace(
                DUMMY_CLUSTER_NAME, self.cluster_identifier)
            os.remove(os.path.join(cur_dir, yaml_file_name))
        except OSError:
            pass

    def get_admin_addr(self):
        if self.service_types['management'] == LOAD_BALANCER:
            return "{host}:{port}".format(host=self.clipper_management_ip,
                                          port=self.clipper_management_port)

        if self.use_k8s_proxy:
            return ("{proxy_addr}/api/v1/namespaces/{ns}/"
                    "services/mgmt-frontend-at-{cluster}:{port}/proxy").format(
                        proxy_addr=self.kubernetes_proxy_addr,
                        ns=self.k8s_namespace,
                        cluster=self.cluster_name,
                        port=CLIPPER_INTERNAL_MANAGEMENT_PORT)

        else:
            return "{host}:{port}".format(
                host=self.external_node_hosts[0],
                port=self.clipper_management_port)

    def get_query_addr(self):
        if self.service_types['query'] == LOAD_BALANCER:
            return "{host}:{port}".format(host=self.clipper_query_ip,
                                          port=self.clipper_query_port)

        if self.use_k8s_proxy:
            return (
                "{proxy_addr}/api/v1/namespaces/{ns}/"
                "services/query-frontend-at-{cluster}:{port}/proxy").format(
                    proxy_addr=self.kubernetes_proxy_addr,
                    ns=self.k8s_namespace,
                    cluster=self.cluster_name,
                    port=CLIPPER_INTERNAL_QUERY_PORT)
        else:
            return "{host}:{port}".format(
                host=self.external_node_hosts[0], port=self.clipper_query_port)

    def get_metric_addr(self):
        if self.service_types['metric'] == LOAD_BALANCER:
            return "{host}:{port}".format(host=self.clipper_metric_ip,
                                          port=self.clipper_metric_port)

        if self.use_k8s_proxy:
            return ("{proxy_addr}/api/v1/namespaces/{ns}/"
                    "services/metrics-at-{cluster}:{port}/proxy").format(
                        proxy_addr=self.kubernetes_proxy_addr,
                        ns=self.k8s_namespace,
                        cluster=self.cluster_name,
                        port=CLIPPER_INTERNAL_METRIC_PORT)
        else:
            return "{host}:{port}".format(
                host=self.external_node_hosts[0],
                port=self.clipper_metric_port)


def get_model_deployment_name(name, version, query_frontend_id, cluster_name):
    return "{name}-{version}-deployment-at-{query_frontend_id}-at-{cluster_name}".format(
        name=name,
        version=version,
        query_frontend_id=query_frontend_id,
        cluster_name=cluster_name)
