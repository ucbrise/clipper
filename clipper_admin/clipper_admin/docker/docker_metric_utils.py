import yaml
import requests
import random
import os


def ensure_clipper_tmp():
    try:
        os.makedirs('/tmp/clipper')
    except OSError as e:
        # Equivalent to os.makedirs(., exist_ok=True) in py3
        pass


def get_prometheus_base_config():
    conf = dict()
    conf['global'] = {'evaluation_interval': '5s', 'scrape_interval': '5s'}
    conf['scrape_configs'] = []
    return conf


def run_query_frontend_metric_image(name, docker_client, query_name,
                                    common_labels, extra_container_kwargs):
    query_frontend_metric_cmd = "--query_frontend_name {}".format(query_name)
    query_frontend_metric_labels = common_labels.copy()

    docker_client.containers.run(
        "clipper/frontend-exporter",
        query_frontend_metric_cmd,
        name=name,
        labels=query_frontend_metric_labels,
        **extra_container_kwargs)


def setup_metric_config(query_frontend_metric_name,
                        CLIPPER_INTERNAL_METRIC_PORT):
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


def run_metric_image(docker_client, common_labels, extra_container_kwargs):
    metric_cmd = [
        "--config.file=/etc/prometheus/prometheus.yml",
        "--storage.tsdb.path=/prometheus",
        "--web.console.libraries=/etc/prometheus/console_libraries",
        "--web.console.templates=/etc/prometheus/consoles",
        "--web.enable-lifecycle"
    ]
    metric_labels = common_labels.copy()
    docker_client.containers.run(
        "prom/prometheus",
        metric_cmd,
        name="metric_frontend-{}".format(random.randint(0, 100000)),
        ports={'9090/tcp': 9090},
        volumes={
            '/tmp/clipper/prometheus.yml': {
                'bind': '/etc/prometheus/prometheus.yml',
                'mode': 'ro'
            }
        },
        labels=metric_labels,
        **extra_container_kwargs)


def update_metric_config(model_container_name, CLIPPER_INTERNAL_METRIC_PORT):
    with open('/tmp/clipper/prometheus.yml', 'r') as f:
        conf = yaml.load(f)

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
