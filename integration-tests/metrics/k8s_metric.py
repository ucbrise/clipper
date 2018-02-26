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
from test_utils import log_clipper_state, create_kubernetes_connection
from metric_utils import parse_res_and_assert_node, get_matched_query, get_metrics_config

sys.path.insert(0, os.path.abspath("%s/../../clipper_admin" % cur_dir))
from clipper_admin import ClipperConnection, KubernetesContainerManager


def predict(addr, x):
    url = "http://%s/simple-example/predict" % addr
    req_json = json.dumps({'input': list(x)})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)
    if r.status_code == requests.codes.ok:
        print(r.json())


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

    logger.info("Start K8s Metric Test (0/1): Running 2 Replicas")

    clipper_conn = create_kubernetes_connection(
        cleanup=True, start_clipper=True)
    time.sleep(10)

    print(clipper_conn.cm.get_query_addr())
    print(clipper_conn.inspect_instance())

    query_addr = clipper_conn.cm.get_query_addr()
    metric_addr = clipper_conn.cm.get_metric_addr()

    app_name = 'simple-example'
    model_name = 'simple-example'
    clipper_conn.register_application(app_name, "doubles", "default_pred",
                                      100000)
    clipper_conn.build_and_deploy_model(
        model_name,
        1,
        "doubles",
        'data',
        "clipper/noop-container",
        num_replicas=2,
        container_registry=
        "568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper")
    clipper_conn.link_model_to_app(app_name, model_name)

    time.sleep(30)
    try:
        logger.info(
            "Making 100 predictions using two model container; Should takes 24 seconds."
        )
        for _ in range(100):
            predict(query_addr, np.random.random(200))
            time.sleep(0.2)

        logger.info("Test 1: Checking status of 3 node exporter")
        up_response = get_matched_query(
            query_request_template.format(metric_addr, 'up'))
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
                query_request_template.format(metric_addr, name))
            parse_res_and_assert_node(res, node_num=2)
        logger.info("Test 2 Passed")

        logger.info("K8s Metric Test Done, Cleaning up...")
        clipper_conn.stop_all()
    except Exception as e:
        logger.error(e)
        log_clipper_state(clipper_conn)
        clipper_conn.stop_all()
        sys.exit(1)
