from __future__ import absolute_import, division, print_function
from ..container_manager import (create_model_container_label, ContainerManager,
                                 CLIPPER_DOCKER_LABEL, CLIPPER_MODEL_CONTAINER_LABEL)
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


class K8sContainerManager(ContainerManager):
    def __init__(self, k8s_api_ip,
                 redis_ip=None,
                 redis_port=6379,
                 start_local_registry=False):

        self.k8s_api_ip = k8s_api_ip
        self.redis_ip = redis_ip
        self.redis_port = redis_port

        config.load_kube_config()
        configuration.assert_hostname = False
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()
        if start_local_registry:
            self._start_registry()
        # else:
        #     # TODO: test with provided registry
        #     self.registry = registry
        #     if registry_username is not None and registry_password is not None:
        #         if "DOCKER_API_VERSION" in os.environ:
        #             self.docker_client = docker.from_env(
        #                 version=os.environ["DOCKER_API_VERSION"])
        #         else:
        #             self.docker_client = docker.from_env()
        #         logger.info("Logging in to {registry} as {user}".format(
        #             registry=registry, user=registry_username))
        #         login_response = self.docker_client.login(
        #             username=registry_username,
        #             password=registry_password,
        #             registry=registry)
        #         logger.info(login_response)

    def _start_registry(self):
        """

        Returns
        -------
        str
            The address of the registry
        """
        logger.info("Initializing Docker registry on k8s cluster")
        with _pass_conflicts():
            self._k8s_v1.create_namespaced_replication_controller(
                body=yaml.load(
                    open(
                        os.path.join(
                            cur_dir,
                            'kube-registry-replication-controller.yaml'))),
                namespace='kube-system')
        with _pass_conflicts():
            self._k8s_v1.create_namespaced_service(
                body=yaml.load(
                    open(os.path.join(cur_dir, 'kube-registry-service.yaml'))),
                namespace='kube-system')
        with _pass_conflicts():
            self._k8s_beta.create_namespaced_daemon_set(
                body=yaml.load(
                    open(
                        os.path.join(cur_dir,
                                     'kube-registry-daemon-set.yaml'))),
                namespace='kube-system')
        # return "{}:5000".format(self.k8s_api_ip)
        # return "localhost:5000"

    def start_clipper(self,
                      query_frontend_image,
                      mgmt_frontend_image):
        """Deploys Clipper to the k8s cluster and exposes the frontends as services."""
        logger.info("Initializing Clipper services to k8s cluster")
        # if an existing Redis service isn't provided, start one
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
        logger.info("Found {num_nodes} nodes: {nodes}".format(num_nodes=len(external_node_hosts),
                                                              nodes=", ".join(external_node_hosts)))
        mgmt_frontend_ports = self._k8s_v1.read_namespaced_service(
            name="mgmt-frontend", namespace='default').spec.ports
        for p in mgmt_frontend_ports:
            if p.name == "1338":
                self.clipper_management_port = p.node_port
                logger.info("Setting Clipper mgmt port to {}".format(self.clipper_management_port))
        query_frontend_ports = self._k8s_v1.read_namespaced_service(
            name="query-frontend", namespace='default').spec.ports
        for p in query_frontend_ports:
            if p.name == "1337":
                self.clipper_query_port = p.node_port
                logger.info("Setting Clipper query port to {}".format(self.clipper_query_port))
            elif p.name == "7000":
                self.clipper_rpc_port = p.node_port

    def deploy_model(self, name, version, input_type, image, num_replicas=1):
        """Deploys a versioned model to a k8s cluster.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        image : str
            A docker repository path, which must be accessible by the k8s cluster.
        """
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
                                    CLIPPER_DOCKER_LABEL: ""
                                }
                            },
                            'spec': {
                                'containers': [{
                                    'name': deployment_name,
                                    'image': image,
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
            with open("deploy.yaml", "w") as f:
                yaml.dump(body, f)
            self._k8s_beta.create_namespaced_deployment(
                body=body,
                namespace='default')

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
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL).items:
            for c in pod.status.container_statuses:
                log_file_name = "image_{image}:container_{id}.log".format(
                    image=c.image_id, id=c.container_id)
                log_file = os.path.join(logging_dir, log_file_name)
                with open(log_file, "w") as lf:
                    lf.write(
                        self._k8s_v1.read_namespaced_pod_log(
                            namespace='default',
                            name=pod.metadata.name,
                            container=c.name))
                log_files.append(log_file)
        return log_files

    def stop_models(self, model_name=None, keep_version=None):
        # TODO(feynman): Account for model_name and keep_version.
        # NOTE: the format of the value of CLIPPER_MODEL_CONTAINER_LABEL
        # is "model_name:model_version"
        """Stops all deployments of pods running Clipper models."""
        logger.info("Stopping all running Clipper model deployments")
        try:
            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default',
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logger.warn("Exception deleting k8s deployments: {}".format(e))

    def stop_clipper(self):
        # TODO: fix this function
        """Stops all Clipper resources.

        WARNING: Data stored on an in-cluster Redis deployment will be lost!
        This method does not delete any existing in-cluster Docker registry.
        """
        logger.info("Stopping all running Clipper resources")

        try:
            for service in self._k8s_v1.list_namespaced_service(
                    namespace='default',
                    label_selector=CLIPPER_DOCKER_LABEL).items:
                # TODO: use delete collection of services if API provides
                service_name = service.metadata.name
                self._k8s_v1.delete_namespaced_service(
                    namespace='default', name=service_name)

            # for deployment in self._k8s_beta.list_namespaced_deployment(
            #         namespace='default',
            #         label_selector=CLIPPER_DOCKER_LABEL).items:
            #     deployment_name = deployment.metadata.name
            #     logger.info("Deployment name: {}".format(deployment_name))
            #     self._k8s_beta.delete_namespaced_deployment(
            #         namespace='default',
            #         name=deployment_name,
            #         body=client.V1DeleteOptions()
            #         # propagation_policy="foreground"
            #     )

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
            logging.warn("Exception deleting k8s resources: {}".format(e))

    def get_registry(self):
        return self.registry

    def get_admin_addr(self):
        return "{host}:{port}".format(
            host=self.external_node_hosts[0], port=self.clipper_management_port)

    def get_query_addr(self):
        return "{host}:{port}".format(
            host=self.external_node_hosts[0], port=self.clipper_query_port)


def get_model_deployment_name(name, version):
    return "{name}-{version}-deployment".format(name=name, version=version)
