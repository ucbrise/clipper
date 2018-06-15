# This integration test is taken straight fron `kubernetes_integration_test.py
# With the setting 1 app, 1 model, 1 replica.
# Adding multiple frontend checks (2 frontend)

from __future__ import absolute_import, division, print_function
import os
import sys
import requests
import tempfile
import shutil
import json
import numpy as np
import time
import logging
import yaml
from test_utils import (create_kubernetes_connection, BenchmarkException,
                        fake_model_data, headers, log_clipper_state,
                        CLIPPER_CONTAINER_REGISTRY)

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin import __version__ as clipper_version, CLIPPER_TEMP_DIR, ClipperException, __registry__ as clipper_registry

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)
MAX_RETRY = 5


def deploy_model(clipper_conn, name, link=False):
    app_name = "{}-app".format(name)
    model_name = "{}-model".format(name)
    clipper_conn.build_and_deploy_model(
        model_name,
        str(int(time.time())),  # random string as version
        "doubles",
        fake_model_data,
        "{}/noop-container:{}".format(clipper_registry, clipper_version),
        num_replicas=1,
        container_registry=CLIPPER_CONTAINER_REGISTRY)
    time.sleep(10)

    if link:
        clipper_conn.link_model_to_app(app_name, model_name)

    success = False
    num_tries = 0

    while not success and num_tries < MAX_RETRY:
        time.sleep(30)
        num_preds = 25
        num_defaults = 0
        addr = clipper_conn.get_query_addr()
        for i in range(num_preds):
            response = requests.post(
                "http://%s/%s/predict" % (addr, app_name),
                headers=headers,
                data=json.dumps({
                    'input': [0.1, 0.2]
                }))
            result = response.json()
            print(result)
            if response.status_code == requests.codes.ok and result["default"]:
                num_defaults += 1
        if num_defaults > 0:
            logger.error("Error: %d/%d predictions were default" %
                         (num_defaults, num_preds))
        if num_defaults < num_preds / 2:
            success = True

        num_tries += 1

    if not success:
        raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                 (app_name, model_name, 1))


def create_and_test_app(clipper_conn, name):
    app_name = "{}-app".format(name)
    clipper_conn.register_application(app_name, "doubles", "default_pred",
                                      100000)
    time.sleep(1)

    addr = clipper_conn.get_query_addr()
    response = requests.post(
        "http://%s/%s/predict" % (addr, app_name),
        headers=headers,
        data=json.dumps({
            'input': [0.1, 0.2]
        }))
    response.json()
    if response.status_code != requests.codes.ok:
        logger.error("Error: %s" % response.text)
        raise BenchmarkException("Error creating app %s" % app_name)

    deploy_model(clipper_conn, name, link=True)


if __name__ == "__main__":
    import random

    cluster_name = "k8-frontx-{}".format(random.randint(0, 5000))
    try:
        clipper_conn = create_kubernetes_connection(
            cleanup=False,
            start_clipper=True,
            num_frontend_replicas=2,
            new_name=cluster_name)
        time.sleep(10)
        print(clipper_conn.cm.get_query_addr())
        try:
            create_and_test_app(clipper_conn, "testapp0")

            logger.info("Begin Kubernetes Multiple Frontend Test")

            k8s_beta = clipper_conn.cm._k8s_beta
            if (k8s_beta.read_namespaced_deployment(
                    'query-frontend-0-at-{}'.format(cluster_name),
                    namespace='default').to_dict()['status']
                ['available_replicas'] != 1):
                raise BenchmarkException(
                    "Wrong number of replicas of query-frontend-0."
                    "Expected {}, found {}".format(
                        1,
                        k8s_beta.read_namespaced_deployment(
                            'query-frontend-0', namespace='default').to_dict()[
                                'status']['available_replicas']))
            if (k8s_beta.read_namespaced_deployment(
                    'query-frontend-1-at-{}'.format(cluster_name),
                    namespace='default').to_dict()['status']
                ['available_replicas'] != 1):
                raise BenchmarkException(
                    "Wrong number of replicas of query-frontend-1."
                    "Expected {}, found {}".format(
                        1,
                        k8s_beta.read_namespaced_deployment(
                            'query-frontend-1', namespace='default').to_dict()[
                                'status']['available_replicas']))
            logger.info("Ok: we have 2 query frontend depolyments")

            k8s_v1 = clipper_conn.cm._k8s_v1
            svc_lists = k8s_v1.list_namespaced_service(
                namespace='default').to_dict()['items']
            svc_names = [svc['metadata']['name'] for svc in svc_lists]
            if not ('query-frontend-0-at-{}'.format(cluster_name) in svc_names
                    and 'query-frontend-1-at-{}'.format(
                        cluster_name) in svc_names):
                raise BenchmarkException(
                    "Error creating query frontend RPC services")
            logger.info("Ok: we have 2 query-frontend rpc services")

            if not os.path.exists(CLIPPER_TEMP_DIR):
                os.makedirs(CLIPPER_TEMP_DIR)
            tmp_log_dir = tempfile.mkdtemp(dir=CLIPPER_TEMP_DIR)
            logger.info(clipper_conn.get_clipper_logs(tmp_log_dir))

            # Remove temp files
            shutil.rmtree(tmp_log_dir)
            log_clipper_state(clipper_conn)
            logger.info("SUCCESS")
            clipper_conn.stop_all()
        except BenchmarkException as e:
            log_clipper_state(clipper_conn)
            logger.exception("BenchmarkException")
            create_kubernetes_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
            sys.exit(1)
        except ClipperException as e:
            log_clipper_state(clipper_conn)
            logger.exception("ClipperException")
            create_kubernetes_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
            sys.exit(1)
    except Exception as e:
        logger.exception("Exception: {}".format(e))
        sys.exit(1)
