from __future__ import print_function

import json
import logging
import os
import sys
import time
import random
import unittest
from requests.exceptions import ConnectionError

import numpy as np
import requests
from test_utils import (
    log_clipper_state, create_docker_connection,
    get_docker_client, get_one_container,
    get_containers, check_container_logs,
    get_new_connection_instance
)

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin.deployers import python as python_deployer
from clipper_admin import ClipperException


CLIPPER_NODES = [
    'metric_frontend',
    'query_frontend_exporter',
    'query_frontend',
    'mgmt_frontend',
    'redis',
    'fluentd'
]


def predict(addr, x):
    url = "http://%s/simple-example/predict" % addr
    req_json = json.dumps({'input': list(x)})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


class FluentdTest(unittest.TestCase):
    def setUp(self):
        logging.basicConfig(
            format=
            '%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
            datefmt='%y-%m-%d:%H:%M:%S',
            level=0)

        self.logger = logging.getLogger(__name__)
        self.cluster_name = "fluentd-test-{}".format(random.randint(0, 50000))
        self.start_clipper(self.cluster_name)

    def start_clipper(self, cluster_name, use_centralized_log=True):
        self.clipper_conn = create_docker_connection(
            cleanup=False, start_clipper=True, new_name=cluster_name, use_centralized_log=use_centralized_log)

        # Make sure all the containers are on.
        timeout_count = 0
        while True:
            containers = get_containers(self.clipper_conn)
            if self.all_containers_found(containers):
                break
            timeout_count += 1
            if timeout_count == 5:
                self.logger.info("Running containers: {}".format(containers))
                raise TimeoutError(
                    "Containers haven't been created within 10 seconds. "
                    "It means that every instance haven't been initialized."
                )
            time.sleep(2)

        self.logger.info("All the containers are found")

    def doCleanups(self):
        self.clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=self.cluster_name)

    def check_fluentd_has_correct_logs(self, clipper_conn):
        fluentd_container = get_one_container('fluentd', clipper_conn)
        fluentd_logs = str(fluentd_container.logs())
        for node_name in CLIPPER_NODES:
            if node_name == 'query_frontend_exporter':
                continue # We don't check exporter log because it is uncommon.
            self.assertTrue(check_container_logs(fluentd_logs, node_name))

    def check_fluentd_has_correct_model_logs(self, clipper_conn, model_name):
        fluentd_container = get_one_container('fluentd', clipper_conn)
        fluentd_logs = str(fluentd_container.logs())
        self.assertTrue(check_container_logs(fluentd_logs, model_name))

    def all_containers_found(self, containers):
        for c in containers:
            node_name = c.name.split('-')[0]
            if node_name not in CLIPPER_NODES:
                return False

        return True

    def test_invalid_clipper_conn_old_connection_use_log_centralization(self):
        # When new connection doesn't use log-centralization, although
        # the original connection uses log-centralization.
        new_conn = get_new_connection_instance(self.cluster_name, False)
        new_conn.connect()
        self.assertTrue(new_conn.cm.centralize_log)

    def test_invalid_clipper_conn_old_connection_not_use_log_centralization(self):
        # Raise a ClipperException when new connection uses log-centralization, although
        # the original connection does not use log-centralization.
        # Recreate a cluster with
        self.clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=self.cluster_name)
        self.start_clipper(self.cluster_name, use_centralized_log=False)
        new_conn = get_new_connection_instance(self.cluster_name, True)
        self.assertRaises(ClipperException, new_conn.connect)

    def test_correct_fluentd_connection(self):
        new_clipper_conn = get_new_connection_instance(self.cluster_name, use_centralized_log=True)
        new_clipper_conn.connect()

        self.assertTrue(new_clipper_conn.cm.centralize_log)
        self.assertTrue(new_clipper_conn.cm.log_config == new_clipper_conn.cm.logging_system_instance.get_log_config())

        old_conn_fluentd = self.clipper_conn.cm.logging_system_instance
        new_conn_fluentd = new_clipper_conn.cm.logging_system_instance

        self.assertTrue(old_conn_fluentd.port == new_conn_fluentd.port)
        self.assertTrue(old_conn_fluentd.conf_path == new_conn_fluentd.conf_path)

    def test_clipper_with_fluentd(self):
        self.check_fluentd_has_correct_logs(self.clipper_conn)

    def test_deployed_models_are_logged(self):
        # Deploy models
        python_deployer.create_endpoint(
            self.clipper_conn, "simple-example", "doubles", feature_sum, num_replicas=2)
        time.sleep(2)

        self.logger.info(
            "Making 100 predictions using two model container; Should takes 25 seconds."
        )
        for _ in range(100):
            predict(self.clipper_conn.get_query_addr(), np.random.random(200))
            time.sleep(0.2)

        self.check_fluentd_has_correct_model_logs(self.clipper_conn, 'simple-example')


if __name__ == '__main__':
    TEST = [
        'test_invalid_clipper_conn_old_connection_use_log_centralization',
        'test_invalid_clipper_conn_old_connection_not_use_log_centralization',
        'test_correct_fluentd_connection',
        'test_clipper_with_fluentd',
        'test_deployed_models_are_logged'
    ]
    suite = unittest.TestSuite()

    for test in TEST:
        suite.addTest(FluentdTest(test))

    result = unittest.TextTestRunner(verbosity=2, failfast=True).run(suite)
    sys.exit(not result.wasSuccessful())
