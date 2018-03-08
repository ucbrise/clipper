from __future__ import print_function

import json
import logging
import os
import subprocess
import sys
import time
import pprint

import numpy as np
import requests
import yaml

cur_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, '{}/../'.format(cur_dir))
from test_utils import log_clipper_state
from metric_utils import parse_res_and_assert_node, get_matched_query, get_metrics_config

sys.path.insert(0, os.path.abspath("%s/../../clipper_admin" % cur_dir))
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer

DOCKER_METRIC_ADDR = 'localhost:9090'


def predict(addr, x):
    url = "http://%s/simple-example/predict" % addr
    req_json = json.dumps({'input': list(x)})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


def log_docker_ps(clipper_conn):
    container_runing = clipper_conn.cm.docker_client.containers.list()
    logger.info('Current docker status')
    for cont in container_runing:
        logger.info('Name {}, Image {}, Status {}, Label {}'.format(
            cont.name, cont.image, cont.status, cont.labels))


if __name__ == '__main__':
    query_request_template = "http://{}/api/v1/series?match[]={}"

    logging.basicConfig(
        format=
        '%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
        datefmt='%y-%m-%d:%H:%M:%S',
        level=0)

    logger = logging.getLogger(__name__)

    logger.info("Start Docker Metric Test (0/1): Running 2 Replicas")
    clipper_conn = ClipperConnection(DockerContainerManager(redis_port=6380))
    clipper_conn.start_clipper()
    python_deployer.create_endpoint(
        clipper_conn, "simple-example", "doubles", feature_sum, num_replicas=2)
    time.sleep(2)
    try:
        logger.info(
            "Making 100 predictions using two model container; Should takes 25 seconds."
        )
        for _ in range(100):
            predict(clipper_conn.get_query_addr(), np.random.random(200))
            time.sleep(0.2)

        logger.info("Test 1: Checking status of 3 node exporter")
        up_response = get_matched_query(
            query_request_template.format(DOCKER_METRIC_ADDR, 'up'))
        parse_res_and_assert_node(up_response, node_num=3)
        logger.info("Test 1 Passed")

        logger.info("Test 2: Checking Model Container Metrics")
        conf = get_metrics_config()
        conf = conf['Model Container']
        prefix = 'clipper_{}_'.format(conf.pop('prefix'))
        for name, spec in conf.items():
            name = prefix + name
            if spec['type'] == 'Histogram' or spec['type'] == 'Summary':
                name += '_sum'

            res = get_matched_query(
                query_request_template.format(DOCKER_METRIC_ADDR, name))
            parse_res_and_assert_node(res, node_num=2)
        logger.info("Test 2 Passed")

        logger.info("Docker Metric Test Done, Cleaning up...")
        clipper_conn.stop_all()
    except Exception as e:
        log_docker_ps(clipper_conn)
        logger.error(e)
        log_clipper_state(clipper_conn)
        clipper_conn.stop_all()
        sys.exit(1)
