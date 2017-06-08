"""Clipper Kubernetes Utilities"""
# TODO: include labels (used by clipper.stop_all)
# TODO: deletion methods

from contextlib import contextmanager
from kubernetes import client, config
from kubernetes.client.rest import ApiException
import logging
import json
import yaml

import clipper_manager


@contextmanager
def _pass_conflicts():
    try:
        yield
    except ApiException as e:
        body = json.loads(e.body)
        if body['reason'] == 'AlreadyExists':
            logging.info(
                "{} already exists, skipping!".format(body['details']))
            pass


class ClipperK8s:
    # TODO: subclass ContainerManager interface
    def __init__(self):
        config.load_kube_config()
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()

    def start(self):
        self._start_clipper()

        # NOTE: this provides a minikube accessible docker registry for convenience during dev
        # TODO: should not be required, as `deploy_model` should be able to pull from any accessible `repo`
        self._start_registry()

    def deploy_model(self, name, version, repo):
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
                        'replicas': 1,
                        'template': {
                            'metadata': {
                                'labels': {
                                    clipper_manager.CLIPPER_MODEL_CONTAINER_LABEL:
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

    def stop_all_model_deployments(self):
        """Stops all deployments of pods running Clipper models."""
        logging.info("Stopping all running Clipper model deployments")
        try:
            resp = self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default',
                label_selector=clipper_manager.CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logging.warn("Exception deleting k8s deployments: {}".format(e))

    def stop_clipper_resources(self):
        """Stops all Clipper resources.

        WARNING: Data stored on an in-cluster Redis deployment will be lost! This method does not delete
        any existing in-cluster Docker registry.
        """
        logging.info("Stopping all running Clipper resources")

        try:
            for service in self._k8s_v1.list_namespaced_service(
                    namespace='default',
                    label_selector=clipper_manager.CLIPPER_DOCKER_LABEL).items:
                # TODO: use delete collection of services if API provides
                service_name = service.metadata.name
                self._k8s_v1.delete_namespaced_service(
                    namespace='default', name=service_name)

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default',
                label_selector=clipper_manager.CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default',
                label_selector=clipper_manager.CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default',
                label_selector=clipper_manager.CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logging.warn("Exception deleting k8s resources: {}".format(e))

    def _start_clipper(self):
        """Deploys Clipper to the k8s cluster and exposes the frontends as services."""
        logging.info("Initializing Clipper services to k8s cluster")
        for name in ['mgmt-frontend', 'query-frontend', 'redis']:
            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=yaml.load(
                        open('k8s/clipper/{}-deployment.yaml'.format(name))),
                    namespace='default')
            with _pass_conflicts():
                self._k8s_v1.create_namespaced_service(
                    body=yaml.load(
                        open('k8s/clipper/{}-service.yaml'.format(name))),
                    namespace='default')

    def _start_registry(self):
        logging.info("Initializing Docker registry on k8s cluster")
        with _pass_conflicts():
            self._k8s_v1.create_namespaced_replication_controller(
                body=yaml.load(
                    open(
                        'k8s/minikube-registry/kube-registry-replication-controller.yaml'
                    )),
                namespace='kube-system')
        with _pass_conflicts():
            self._k8s_v1.create_namespaced_service(
                body=yaml.load(
                    open('k8s/minikube-registry/kube-registry-service.yaml')),
                namespace='kube-system')
        with _pass_conflicts():
            self._k8s_beta.create_namespaced_daemon_set(
                body=yaml.load(
                    open('k8s/minikube-registry/kube-registry-daemon-set.yaml')
                ),
                namespace='kube-system')
