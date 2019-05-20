from __future__ import print_function

import json
import logging
import os
import subprocess
import sys
import time
import random

import numpy as np
import requests
import yaml
from test_utils import log_clipper_state, create_docker_connection

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer


def predict(addr, x):
    url = "http://%s/simple-example/predict" % addr
    req_json = json.dumps({'input': list(x)})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


def get_metrics_config():
    config_path = os.path.join(
        os.path.abspath("%s/../monitoring" % cur_dir), 'metrics_config.yaml')
    with open(config_path, 'r') as f:
        conf = yaml.load(f, Loader=yaml.FullLoader)
    return conf


def get_matched_query(metric_addr, metric_name):
    query = gen_match_query(metric_addr, metric_name)
    logger.info("Querying: {}".format(query))
    logger.info('test why docker query is broken: {}'.format(repr(query)))
    res = requests.get(query).json()
    logger.info(res)
    return res


def parse_res_and_assert_node(res, node_num):
    assert res['status'] == 'success'
    assert len(res['data']) == node_num


def check_target_health(metric_addr):
    query = metric_addr + '/api/v1/targets'
    logger.info("Querying: {}".format(query))
    res = requests.get(query).json()
    logger.info(res)
    assert res['status'] == 'success'

    active_targets = res['data']['activeTargets']
    assert len(active_targets) == 3, 'Wrong number of targets'

    for target in active_targets:
        assert target['health'] == 'up', "Target {} is not up!".format(target)


def log_docker_ps(clipper_conn):
    container_runing = clipper_conn.cm.docker_client.containers.list()
    logger.info('Current docker status')
    for cont in container_runing:
        logger.info('Name {}, Image {}, Status {}, Label {}'.format(
            cont.name, cont.image, cont.status, cont.labels))


if __name__ == '__main__':
    # metric_addr = "http://localhost:9090"
    gen_match_query = lambda addr, name: "http://{addr}/api/v1/series?match[]={name}".format(addr=addr, name=name)

    logging.basicConfig(
        format=
        '%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
        datefmt='%y-%m-%d:%H:%M:%S',
        level=0)

    logger = logging.getLogger(__name__)

    logger.info("Start Metric Test (0/1): Running 2 Replicas")

    cluster_name = "metric-d-{}".format(random.randint(0, 50000))
    clipper_conn = create_docker_connection(
        cleanup=False, start_clipper=True, new_name=cluster_name)
    python_deployer.create_endpoint(
        clipper_conn, "simple-example", "doubles", feature_sum, num_replicas=2)
    time.sleep(10)
    try:
        logger.info(
            "Making 100 predictions using two model container; Should takes 50 seconds."
        )
        for _ in range(100):
            predict(clipper_conn.get_query_addr(), np.random.random(200))
            time.sleep(0.5)

        logger.info("Test 1: Checking status of 3 node exporter")
        check_target_health("http://{}".format(clipper_conn.cm.get_metric_addr()))
        logger.info("Test 1 Passed")

        logger.info("Test 2: Checking Model Container Metrics")
        conf = get_metrics_config()
        conf = conf['Model Container']
        prefix = 'clipper_{}_'.format(conf.pop('prefix'))
        for name, spec in conf.items():
            name = prefix + name
            if spec['type'] == 'Histogram' or spec['type'] == 'Summary':
                name += '_sum'

            res = get_matched_query(clipper_conn.cm.get_metric_addr(), name)
            parse_res_and_assert_node(res, node_num=2)
        logger.info("Test 2 Passed")

        logger.info("Metric Test Done, Cleaning up...")
        create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=cluster_name)
    except Exception as e:
        log_docker_ps(clipper_conn)
        logger.error(e)
        log_clipper_state(clipper_conn)
        clipper_conn.stop_all()
        sys.exit(1)
