from __future__ import print_function

import json
import logging
import os
import sys
import time
import random

import numpy as np
import requests
import yaml
from test_utils import log_clipper_state, create_docker_connection, get_docker_client

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin.deployers import python as python_deployer
from clipper_admin.container_manager import CLIPPER_DOCKER_LABEL


def predict(addr, x):
    url = "http://%s/simple-example/predict" % addr
    req_json = json.dumps({'input': list(x)})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


def setup(clipper_conn):
    containers = get_containers(clipper_conn)
    fluentd_container = None
    for c in containers:
        if 'fluentd-' in c.name:
            fluentd_container = c

    if fluentd_container is None:
        raise AssertionError("Fluentd has not been running")

    return fluentd_container


def get_containers(clipper_conn):
    docker_client = get_docker_client()
    return docker_client.containers.list(
        filters={
            'label': [
                '{key}={val}'.format(
                    key=CLIPPER_DOCKER_LABEL, val=clipper_conn.cm.cluster_name)
            ]
        })


def check_fluentd_has_correct_logs(clipper_conn):
    # We don't check frontend-exporter because it doesn't have lots of logs and is hard to find.
    fluentd_container = setup(clipper_conn)
    fluentd_logs = str(fluentd_container.logs())

    if not check_query_frontend_logs(fluentd_logs):
        raise AssertionError("Query Frontend log is not found")
    if not check_metric_frontend_logs(fluentd_logs):
        raise AssertionError("Metric Frontend log is not found")
    if not check_management_frontend_logs(fluentd_logs):
        raise AssertionError("Management Frontend log is not found")
    if not check_redis_logs(fluentd_logs):
        raise AssertionError("Redis log is not found")

def check_fluentd_has_correct_model_logs(clipper_conn, model_name):
    fluentd_container = setup(clipper_conn)
    fluentd_logs = str(fluentd_container.logs())

    if not check_model_logs(fluentd_logs, model_name):
        raise AssertionError("{model_name} log is not found".format(model_name=model_name))


def check_query_frontend_logs(fluentd_logs):
    return '"container_name":"/query_frontend' in fluentd_logs


def check_metric_frontend_logs(fluentd_logs):
    return '"container_name":"/metric_frontend' in fluentd_logs


def check_redis_logs(fluentd_logs):
    return '"container_name":"/redis' in fluentd_logs

def check_management_frontend_logs(fluentd_logs):
    return '"container_name":"/mgmt_frontend' in fluentd_logs


def check_model_logs(fluentd_logs, model_name):
    return '"container_name":"/{}',format(model_name) in fluentd_logs


def log_docker_ps(clipper_conn):
    container_runing = clipper_conn.cm.docker_client.containers.list()
    logger.info('Current docker status')
    for cont in container_runing:
        logger.info('Name {}, Image {}, Status {}, Label {}'.format(
            cont.name, cont.image, cont.status, cont.labels))

def all_containers_found(containers):
    metric_frontend_found = False
    management_frontend_found = False
    query_frontend_found = False
    fluentd_found = False
    redis_found = False
    frontend_exporter_found = False

    for c in containers:
        if 'metric_frontend' in c.name:
            metric_frontend_found = True
        elif 'query_frontend_exporter' in c.name:
            frontend_exporter_found = True
        elif 'query_frontend' in c.name:
            query_frontend_found = True
        elif 'mgmt_frontend' in c.name:
            management_frontend_found = True
        elif 'redis' in c.name:
            redis_found = True
        elif 'fluentd' in c.name:
            fluentd_found = True

    return metric_frontend_found and management_frontend_found \
            and query_frontend_found and fluentd_found \
            and redis_found and frontend_exporter_found


def get_newline_str():
    return '=================================================================\n'

if __name__ == '__main__':
    logging.basicConfig(
        format=
        '%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
        datefmt='%y-%m-%d:%H:%M:%S',
        level=0)

    logger = logging.getLogger(__name__)

    logger.info("Start Fluentd Test (0/2): Running 2 Replicas")

    cluster_name = "fluentd-test-{}".format(random.randint(0, 50000))
    clipper_conn = create_docker_connection(
        cleanup=False, start_clipper=True, new_name=cluster_name, use_centralized_log=True)

    try:
        timeout_count = 0
        while True:
            containers = get_containers(clipper_conn)
            if all_containers_found(containers):
                break
            timeout_count += 1
            if timeout_count == 5:
                raise TimeoutError("Containers haven't been created within 10 seconds")
            time.sleep(2)
        logger.info("Test setup: All the necessary instances found")
        logger.info(get_newline_str())

        logger.info("Test 1: Checking if fluentd has correct logs")
        check_fluentd_has_correct_logs(clipper_conn)
        logger.info("Fluentd Test (1/2): Test 1 passed")
        logger.info(get_newline_str())

        logger.info("Test 2: Deploying two models")
        python_deployer.create_endpoint(
            clipper_conn, "simple-example", "doubles", feature_sum, num_replicas=2)
        time.sleep(2)

        logger.info(
            "Making 100 predictions using two model container; Should takes 25 seconds."
        )
        for _ in range(100):
            predict(clipper_conn.get_query_addr(), np.random.random(200))
            time.sleep(0.2)

        logger.info("Test 2: Checking if fluentd has correct model logs")
        check_fluentd_has_correct_model_logs(clipper_conn, 'simple-example')
        logger.info("Fluentd Test (2/2): Test 2 passed")
        logger.info(get_newline_str())

        create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=cluster_name)

        logger.info("Fluentd tests All passed")
    except Exception as e:
        logger.info("Test failed")
        log_docker_ps(clipper_conn)
        logger.error(e)
        log_clipper_state(clipper_conn)
        clipper_conn.stop_all(graceful=False)
        sys.exit(1)
