"""Clipper Kubernetes Utilities"""
# TODO: better error handling, check if resources exist before creating
# TODO: include labels (used by clipper.stop_all)
# TODO: deletion methods

import yaml
from kubernetes import client, config
from kubernetes.client.rest import ApiException

class ClipperK8s:
    # TODO: subclass ContainerManager interface
    def __init__(self):
        config.load_kube_config()
        # self.initialize_clipper() # NOTE: this allows containers to discover query_manager by DNS rather than IP,
        #                           # but may couple too tightly to k8s
        self.initialize_registry() # TODO: check doesn't exist before trying
        self.k8s_v1 = client.CoreV1Api()
        self.k8s_beta = client.ExtensionsV1beta1Api()

    def initialize_clipper(self):
        for name in ['mgmt-frontend', 'query-frontend', 'redis']:
            try:
                self.k8s_beta.create_namespaced_deployment(
                        body=yaml.load(open('k8s/clipper/{}-deployment.yaml'.format(name))), namespace='default')
            except ApiException:
                pass
            try:
                self.k8s_v1.create_namespaced_service(
                        body=yaml.load(open('k8s/clipper/{}-service.yaml'.format(name))), namespace='default')
            except ApiException:
                pass

    def initialize_registry(self):
        try:
            self.k8s_v1.create_namespaced_replication_controller(
                    body=yaml.load(open('k8s/minikube-registry/kube-registry-rc.yaml')), namespace='kube-system')
        except ApiException: # already exists
            pass
        try:
            self.k8s_v1.create_namespaced_service(
                    body=yaml.load(open('k8s/minikube-registry/kube-registry-svc.yaml')), namespace='kube-system')
        except ApiException: # already exists
            pass
        try:
            self.k8s_beta.create_namespaced_daemon_set(
                    body=yaml.load(open('k8s/minikube-registry/kube-registry-ds.yaml')), namespace='kube-system')
        except ApiException: # already exists
            pass

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
        try:
            k8s_beta.create_namespaced_deployment(
                    body={
                        'apiVersion': 'extensions/v1beta1',
                        'kind': 'Deployment',
                        'metadata': {
                            'name': name + '-deployment' # NOTE: must satisfy RFC 1123 pathname conventions
                        },
                        'spec': {
                            'replicas': 1,
                            'template': {
                                'metadata': {
                                    'labels': {
                                        'model': name,
                                        'version': str(version)
                                    }
                                },
                                'spec': {
                                    'containers': [
                                        {
                                            'name': name,
                                            'image': repo,
                                            'ports': [
                                                {
                                                    'containerPort': 80
                                                }
                                            ],
                                            'env': [
                                                {
                                                    'name': 'CLIPPER_MODEL_NAME',
                                                    'value': 'feature-sum-model'
                                                },
                                                {
                                                    'name': 'CLIPPER_MODEL_VERSION',
                                                    'value': '1'
                                                },
                                                {
                                                    'name': 'CLIPPER_IP',
                                                    'value': '10.0.2.2' # TODO: WTF magic IP that goes to host
                                                }
                                            ]
                                        }
                                    ]
                                }
                            }
                        }
                    }, namespace='default')
        except ApiException: # already exists
            pass
