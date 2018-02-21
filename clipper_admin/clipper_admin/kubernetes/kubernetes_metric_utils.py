import yaml
import os
from kubernetes.client import V1ConfigMap
from contextlib import contextmanager
from kubernetes.client.rest import ApiException
import logging
import json
from ..version import __version__

_cur_dir = os.path.dirname(os.path.abspath(__file__))
prom_deployment_path = os.path.join(_cur_dir, 'prom_deployment.yml')
prom_service_path = os.path.join(_cur_dir, 'prom_service.yml')
prom_configmap_path = os.path.join(_cur_dir, 'prom_configmap.yml')
frontend_exporter_deployment_path = os.path.join(
    _cur_dir, 'frontend-exporter-deployment.yaml')

logger = logging.getLogger(__name__)

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


def _create_prometheus_configmap(_k8s_v1):
    with open(prom_configmap_path, 'r') as f:
        data = yaml.load(f)

    with _pass_conflicts():
        _k8s_v1.create_namespaced_config_map(body=data, namespace='default')


def _create_prometheus_deployment(_k8s_beta):
    with open(prom_deployment_path, 'r') as f:
        data = yaml.load(f)

    with _pass_conflicts():
        _k8s_beta.create_namespaced_deployment(body=data, namespace='default')


def _create_prometheus_service(_k8s_v1):
    with open(prom_service_path, 'r') as f:
        data = yaml.load(f)

    with _pass_conflicts():
        _k8s_v1.create_namespaced_service(body=data, namespace='default')


def start_prometheus(_k8s_v1, _k8s_beta):
    _create_prometheus_configmap(_k8s_v1)
    _create_prometheus_deployment(_k8s_beta)
    _create_prometheus_service(_k8s_v1)
