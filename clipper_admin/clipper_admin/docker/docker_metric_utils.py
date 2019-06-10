import yaml
import requests
from ..exceptions import ClipperException
from ..container_manager import CLIPPER_INTERNAL_QUERY_PORT
from .docker_api_utils import run_container

PROM_VERSION = "v2.9.2"


def _get_prometheus_base_config():
    """
    Generate a basic configuration dictionary for prometheus
    :return: dictionary
    """
    conf = dict()
    conf['global'] = {'evaluation_interval': '5s', 'scrape_interval': '5s'}
    conf['scrape_configs'] = []
    return conf


def run_query_frontend_metric_image(name, docker_client, query_name,
                                    frontend_exporter_image, common_labels,
                                    log_config, extra_container_kwargs):
    """
    Use docker_client to run a frontend-exporter image.
    :param name: Name to pass in, need to be unique.
    :param docker_client: The docker_client object.
    :param query_name: The corresponding frontend name
    :param common_labels: Labels to pass in.
    :param extra_container_kwargs: Kwargs to pass in.
    :return: None
    """

    query_frontend_metric_cmd = "--query_frontend_name {}:{}".format(
        query_name, CLIPPER_INTERNAL_QUERY_PORT)
    query_frontend_metric_labels = common_labels.copy()

    run_container(
        docker_client=docker_client,
        image=frontend_exporter_image,
        cmd=query_frontend_metric_cmd,
        log_config=log_config,
        name=name,
        labels=query_frontend_metric_labels,
        extra_container_kwargs=extra_container_kwargs)


def setup_metric_config(query_frontend_metric_name, prom_config_path,
                        CLIPPER_INTERNAL_METRIC_PORT):
    """
    Write to file prometheus.yml after frontend-metric is setup.
    :param query_frontend_metric_name: Corresponding image name
    :param prom_config_path: Prometheus config file to write in
    :param CLIPPER_INTERNAL_METRIC_PORT: Default port.
    :return: None
    """

    with open(prom_config_path, 'w') as f:
        prom_config = _get_prometheus_base_config()
        prom_config_query_frontend = {
            'job_name':
            'query',
            'static_configs': [{
                'targets': [
                    '{name}:{port}'.format(
                        name=query_frontend_metric_name,
                        port=CLIPPER_INTERNAL_METRIC_PORT)
                ]
            }]
        }
        prom_config['scrape_configs'].append(prom_config_query_frontend)

        yaml.dump(prom_config, f)


def run_metric_image(metric_frontend_name, docker_client, common_labels,
                     prometheus_port, prom_config_path, log_config,
                     extra_container_kwargs):
    """
    Run the prometheus image.
    :param metric_frontend_name: container name
    :param docker_client: The docker client object
    :param common_labels: Labels to pass in
    :param prom_config_path: Where config file lives
    :param extra_container_kwargs: Kwargs to pass in.
    :return: None
    """

    # CMD comes from https://github.com/prometheus/prometheus/blob/release-2.1/Dockerfile
    metric_cmd = [
        "--config.file=/etc/prometheus/prometheus.yml",
        "--storage.tsdb.path=/prometheus",
        "--web.console.libraries=/etc/prometheus/console_libraries",
        "--web.console.templates=/etc/prometheus/consoles",
        "--web.enable-lifecycle"
    ]
    metric_labels = common_labels.copy()

    run_container(
        docker_client=docker_client,
        image="prom/prometheus:{}".format(PROM_VERSION),
        cmd=metric_cmd,
        name=metric_frontend_name,
        ports={'9090/tcp': prometheus_port},
        log_config=log_config,
        volumes={
            prom_config_path: {
                'bind': '/etc/prometheus/prometheus.yml',
                'mode': 'ro'
            }
        },
        user='root',  # prom use nobody by default but it can't access config.
        labels=metric_labels,
        extra_container_kwargs=extra_container_kwargs)


def add_to_metric_config(model_container_name, prom_config_path,
                         prometheus_port, CLIPPER_INTERNAL_METRIC_PORT):
    """
    Add a new model container to the prometheus.yml configuration file.
    :param model_container_name: New model container name, need to be unique.
    :param prom_config_path: Where prometheus config file lives
    :param CLIPPER_INTERNAL_METRIC_PORT: Default port
    :return: None

    Raises
        ------
        :py:exc:`clipper.ClipperException`
    """
    with open(prom_config_path, 'r') as f:
        conf = yaml.load(f, Loader=yaml.FullLoader)

    for config in conf['scrape_configs']:
        if config['job_name'] == model_container_name:
            raise ClipperException(
                '{} is added already on the metric configs'.format(
                    model_container_name))

    new_job_dict = {
        'job_name':
        '{}'.format(model_container_name),
        'static_configs': [{
            'targets': [
                '{name}:{port}'.format(
                    name=model_container_name,
                    port=CLIPPER_INTERNAL_METRIC_PORT)
            ]
        }]
    }
    conf['scrape_configs'].append(new_job_dict)

    with open(prom_config_path, 'w') as f:
        yaml.dump(conf, f)

    requests.post('http://localhost:{prometheus_port}/-/reload'.format(
        prometheus_port=prometheus_port))


def delete_from_metric_config(model_container_name, prom_config_path,
                              prometheus_port):
    """
    Delete the stored model container from the prometheus.yml configuration file.
    :param model_container_name: the model container name to be deleted.
    :return: None
    """
    with open(prom_config_path, 'r') as f:
        conf = yaml.load(f, Loader=yaml.FullLoader)

    for i, config in enumerate(conf['scrape_configs']):
        if config['job_name'] == model_container_name:
            conf['scrape_configs'].pop(i)
            break

    with open(prom_config_path, 'w') as f:
        yaml.dump(conf, f)

    requests.post('http://localhost:{prometheus_port}/-/reload'.format(
        prometheus_port=prometheus_port))
