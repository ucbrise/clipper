import yaml
import requests
import random
import os
from ..exceptions import ClipperException
from ..version import __version__
from ..container_manager import CLIPPER_INTERNAL_QUERY_PORT

PROM_VERSION = "v2.1.0"


def ensure_clipper_tmp():
    """
    Make sure /tmp/clipper directory exist. If not, make one.
    :return: None
    """
    try:
        os.makedirs('/tmp/clipper')
    except OSError as e:
        # Equivalent to os.makedirs(., exist_ok=True) in py3
        pass


def get_prometheus_base_config():
    """
    Generate a basic configuration dictionary for prometheus
    :return: dictionary
    """
    conf = dict()
    conf['global'] = {'evaluation_interval': '5s', 'scrape_interval': '5s'}
    conf['scrape_configs'] = []
    return conf


def run_query_frontend_metric_image(name, docker_client, query_name,
                                    common_labels, extra_container_kwargs):
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

    docker_client.containers.run(
        "clipper/frontend-exporter:{}".format(__version__),
        query_frontend_metric_cmd,
        name=name,
        labels=query_frontend_metric_labels,
        **extra_container_kwargs)


def setup_metric_config(query_frontend_metric_name,
                        CLIPPER_INTERNAL_METRIC_PORT):
    """
    Write to file prometheus.yml after frontend-metric is setup.
    :param query_frontend_metric_name: Corresponding image name
    :param CLIPPER_INTERNAL_METRIC_PORT: Default port.
    :return: None
    """

    ensure_clipper_tmp()

    with open('/tmp/clipper/prometheus.yml', 'w') as f:
        prom_config = get_prometheus_base_config()
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


def run_metric_image(docker_client, common_labels, prometheus_port,
                     extra_container_kwargs):
    """
    Run the prometheus image.
    :param docker_client: The docker client object
    :param common_labels: Labels to pass in
    :param extra_container_kwargs: Kwargs to pass in.
    :return: None
    """
    metric_cmd = [
        "--config.file=/etc/prometheus/prometheus.yml",
        "--storage.tsdb.path=/prometheus",
        "--web.console.libraries=/etc/prometheus/console_libraries",
        "--web.console.templates=/etc/prometheus/consoles",
        "--web.enable-lifecycle"
    ]
    metric_labels = common_labels.copy()
    docker_client.containers.run(
        "prom/prometheus:{}".format(PROM_VERSION),
        metric_cmd,
        name="metric_frontend-{}".format(random.randint(0, 100000)),
        ports={'9090/tcp': prometheus_port},
        volumes={
            '/tmp/clipper/prometheus.yml': {
                'bind': '/etc/prometheus/prometheus.yml',
                'mode': 'ro'
            }
        },
        labels=metric_labels,
        **extra_container_kwargs)


def add_to_metric_config(model_container_name, CLIPPER_INTERNAL_METRIC_PORT):
    """
    Add a new model container to the prometheus.yml configuration file.
    :param model_container_name: New model container name, need to be unique.
    :param CLIPPER_INTERNAL_METRIC_PORT: Default port
    :return: None

    Raises
        ------
        :py:exc:`clipper.ClipperException`
    """
    with open('/tmp/clipper/prometheus.yml', 'r') as f:
        conf = yaml.load(f)

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

    with open('/tmp/clipper/prometheus.yml', 'w') as f:
        yaml.dump(conf, f)

    requests.post('http://localhost:9090/-/reload')


def delete_from_metric_config(model_container_name):
    """
    Delete the stored model container from the prometheus.yml configuration file.
    :param model_container_name: the model container name to be deleted.
    :return: None
    """
    with open('/tmp/clipper/prometheus.yml', 'r') as f:
        conf = yaml.load(f)

    for i, config in enumerate(conf['scrape_configs']):
        if config['job_name'] == model_container_name:
            conf['scrape_configs'].pop(i)
            break

    with open('/tmp/clipper/prometheus.yml', 'w') as f:
        yaml.dump(conf, f)

    requests.post('http://localhost:9090/-/reload')
