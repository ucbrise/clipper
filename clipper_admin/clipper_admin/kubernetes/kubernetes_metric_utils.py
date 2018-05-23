import yaml
import os
from kubernetes.client import V1ConfigMap
from contextlib import contextmanager
from kubernetes.client.rest import ApiException
import logging
import json
from ..version import __version__

_cur_dir = os.path.dirname(os.path.abspath(__file__))
prom_deployment_path = os.path.join(_cur_dir, 'prom_deployment.yaml')
prom_service_path = os.path.join(_cur_dir, 'prom_service.yaml')
prom_configmap_path = os.path.join(_cur_dir, 'prom_configmap.yaml')
frontend_exporter_deployment_path = os.path.join(
    _cur_dir, 'frontend-exporter-deployment.yaml')

logger = logging.getLogger(__name__)

PROM_VERSION = "v2.1.0"
CLIPPER_FRONTEND_EXPORTER_IMAGE = "clipper/frontend-exporter:{}".format(
    __version__)


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


def _create_prometheus_configmap(_k8s_v1, namespace):
    with open(prom_configmap_path, 'r') as f:
        data = yaml.load(f)

    with _pass_conflicts():
        _k8s_v1.create_namespaced_config_map(body=data, namespace=namespace)


def _create_prometheus_deployment(_k8s_beta, namespace):
    with open(prom_deployment_path, 'r') as f:
        data = yaml.load(f)

    data['spec']['template']['spec']['containers'][0][
        'image'] = "prom/prometheus:{version}".format(version=PROM_VERSION)

    with _pass_conflicts():
        _k8s_beta.create_namespaced_deployment(body=data, namespace=namespace)


def _create_prometheus_service(_k8s_v1, namespace):
    with open(prom_service_path, 'r') as f:
        data = yaml.load(f)

    with _pass_conflicts():
        _k8s_v1.create_namespaced_service(body=data, namespace=namespace)


def start_prometheus(_k8s_v1, _k8s_beta, namespace='default'):
    _create_prometheus_configmap(_k8s_v1, namespace)
    _create_prometheus_deployment(_k8s_beta, namespace)
    _create_prometheus_service(_k8s_v1, namespace)
