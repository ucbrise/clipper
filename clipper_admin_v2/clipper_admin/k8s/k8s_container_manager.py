from __future__ import absolute_import, division, print_function
from ..container_manager import (ContainerManager, CLIPPER_DOCKER_LABEL,
                                 CLIPPER_MODEL_CONTAINER_LABEL)

from contextlib import contextmanager
from kubernetes import client, config
from kubernetes.client.rest import ApiException
import logging
import json
import yaml
import docker
import os

logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))


@contextmanager
def _pass_conflicts():
    try:
        yield
    except ApiException as e:
        body = json.loads(e.body)
        if body['reason'] == 'AlreadyExists':
            logger.info(
                "{} already exists, skipping!".format(body['details']))
            pass


class K8sContainerManager(ContainerManager):
    def __init__(self,
                 clipper_public_hostname,
                 **kwargs):
        super(K8sContainerManager, self).__init__(clipper_public_hostname, **kwargs)
        config.load_kube_config()
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()
        if self.registry is None:
            self.registry = self._start_registry()
        else:
            # TODO: test with provided registry
            self.registry = registry
            if registry_username is not None and registry_password is not None:
                if "DOCKER_API_VERSION" in os.environ:
                    self.docker_client = docker.from_env(
                        version=os.environ["DOCKER_API_VERSION"])
                else:
                    self.docker_client = docker.from_env()
                logger.info("Logging in to {registry} as {user}".format(
                    registry=registry, user=registry_username))
                login_response = self.docker_client.login(
                    username=registry_username,
                    password=registry_password,
                    registry=registry)
                logger.info(login_response)

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
                    open(os.path.join(cur_dir, 'kube-registry-replication-controller.yaml'))),
                namespace='kube-system')
        with _pass_conflicts():
            self._k8s_v1.create_namespaced_service(
                body=yaml.load(
                    open(os.path.join(cur_dir, 'kube-registry-service.yaml'))),
                namespace='kube-system')
        with _pass_conflicts():
            self._k8s_beta.create_namespaced_daemon_set(
                body=yaml.load(
                    open(os.path.join(cur_dir, 'kube-registry-daemon-set.yaml'))),
                namespace='kube-system')
        return "localhost:5000"

    def start_clipper(self):
        """Deploys Clipper to the k8s cluster and exposes the frontends as services."""
        logger.info("Initializing Clipper services to k8s cluster")
        # if an existing Redis service isn't provided, start one
        if self.redis_ip is None:
            name = 'redis'
            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=yaml.load(
                        open(os.path.join(cur_dir, '{}-deployment.yaml'.format(name)))),
                    namespace='default')
            with _pass_conflicts():
                body = yaml.load(
                    open(os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                body["spec"]["ports"][0]["port"] = self.redis_port
                self._k8s_v1.create_namespaced_service(
                    body=body,
                    namespace='default')
        for name in ['mgmt-frontend', 'query-frontend']:
            with _pass_conflicts():
                body = yaml.load(
                    open(os.path.join(cur_dir, '{}-deployment.yaml'.format(name))))
                if self.redis_ip is not None:
                    args = ["--redis_ip={}".format(self.redis_ip),
                            "--redis_port={}".format(self.redis_port)]
                    body["spec"]["template"]["spec"]["containers"][0]["args"] = args
                self._k8s_beta.create_namespaced_deployment(
                    body=body,
                    namespace='default')
            with _pass_conflicts():
                body = yaml.load(
                    open(os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                self._k8s_v1.create_namespaced_service(
                    body=body,
                    namespace='default')

    def deploy_model(self, name, version, input_type, repo, num_replicas=1):
        """Deploys a versioned model to a k8s cluster.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        repo : str
            A docker repository path, which must be accessible by the k8s cluster.
        """
        with _pass_conflicts():
            # TODO: handle errors where `repo` is not accessible
            self._k8s_beta.create_namespaced_deployment(
                body={
                    'apiVersion': 'extensions/v1beta1',
                    'kind': 'Deployment',
                    'metadata': {
                        'name': name +
                        '-deployment'  # NOTE: must satisfy RFC 1123 pathname conventions
                    },
                    'spec': {
                        'replicas': num_replicas,
                        'template': {
                            'metadata': {
                                'labels': {
                                    CLIPPER_MODEL_CONTAINER_LABEL:
                                    '',
                                    'model':
                                    name,
                                    'version':
                                    str(version)
                                }
                            },
                            'spec': {
                                'containers': [{
                                    'name':
                                    name,
                                    'image':
                                    repo,
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
                },
                namespace='default')

    def add_replica(self, name, version, input_type, repo):
        """Increments the number of replicas of a model.

        """
        deployment_name = "{}-deployment".format(name) # NOTE: assumes `metadata.name` can identify the model deployment.
        response = self._k8s_beta.read_namespaced_deployments_scale(name=deployment_name, namespace='default')

        current_num_replicas = response.spec.replicas

        self._k8s_beta.patch_namespaced_deployments_scale(
                name=deployment_name,
                namespace='default',
                body={
                    'spec': {
                        'replicas': current_num_replicas + 1,
                    }
                })

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

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default',
                label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default',
                label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default',
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logging.warn("Exception deleting k8s resources: {}".format(e))

    def get_registry(self):
        return self.registry
