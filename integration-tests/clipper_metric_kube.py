# This integration test is taken straight fron `kubernetes_integration_test.py
# With the setting 1 app, 1 model, 2 replica. 
# Adding container monitoring check

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
                        fake_model_data, headers, log_clipper_state)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin import __version__ as clipper_version, CLIPPER_TEMP_DIR, ClipperException

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

MAX_RETRY = 4

# TODO: Add kubernetes specific checks that use kubernetes API


def deploy_model(clipper_conn, name, version, link=False):
    app_name = "%s-app" % name
    model_name = "%s-model" % name
    clipper_conn.build_and_deploy_model(
        model_name,
        version,
        "doubles",
        fake_model_data,
        "clipper/noop-container:{}".format(clipper_version),
        num_replicas=2, # We set it to 2 for metric purpose.
        container_registry=
        "568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper")
    time.sleep(10)

    if link:
        clipper_conn.link_model_to_app(app_name, model_name)

    success = False
    num_tries = 0

    while not success and num_tries < 5:
        time.sleep(30)
        num_preds = 25
        num_defaults = 0
        addr = clipper_conn.get_query_addr()
        for i in range(num_preds):
            response = requests.post(
                "http://%s/%s/predict" % (addr, app_name),
                headers=headers,
                data=json.dumps({
                    'input': list(np.random.random(30))
                }))
            result = response.json()
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
                                 (app_name, model_name, version))


def create_and_test_app(clipper_conn, name, num_models):
    app_name = "%s-app" % name
    clipper_conn.register_application(app_name, "doubles", "default_pred",
                                      100000)
    time.sleep(1)

    addr = clipper_conn.get_query_addr()
    response = requests.post(
        "http://%s/%s/predict" % (addr, app_name),
        headers=headers,
        data=json.dumps({
            'input': list(np.random.random(30))
        }))
    response.json()
    if response.status_code != requests.codes.ok:
        logger.error("Error: %s" % response.text)
        raise BenchmarkException("Error creating app %s" % app_name)

    for i in range(num_models):
        if i == 0:
            deploy_model(clipper_conn, name, i, link=True)
        else:
            deploy_model(clipper_conn, name, i)
        time.sleep(1)

#### Metric Helper
def get_matched_query(addr, keyword):
    query = "{}/api/v1/series?match[]={}".format(addr, keyword)
    
    logger.info("Querying: {}".format(query))
    res = requests.get(query).json()
    logger.info(res)

    return res

def parse_res_and_assert_node(res, node_num):
    assert res['status'] == 'success'
    assert len(res['data']) == node_num

def get_metrics_config():
    config_path = os.path.join(
        os.path.abspath("%s/../monitoring" % cur_dir), 'metrics_config.yaml')
    with open(config_path, 'r') as f:
        conf = yaml.load(f)
    return conf

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

if __name__ == "__main__":
    num_apps = 1
    num_models = 1
    try:
        if len(sys.argv) > 1:
            num_apps = int(sys.argv[1])
        if len(sys.argv) > 2:
            num_models = int(sys.argv[2])
    except IndexError:
        # it's okay to pass here, just use the default values
        # for num_apps and num_models
        pass
    try:
        clipper_conn = create_kubernetes_connection(
            cleanup=True, start_clipper=True)
        time.sleep(10)
        print(clipper_conn.cm.get_query_addr())
        print(clipper_conn.inspect_instance())
        try:
            logger.info("Set up: with %d apps and %d models" %
                        (num_apps, num_models))
            for a in range(num_apps):
                create_and_test_app(clipper_conn, "testapp%s" % a, num_models)

            # Start Metric Check
            metric_api_addr = clipper_conn.cm.get_metric_addr()
            
            # Account for InvalidSchema: No connection adapters were found
            if not metric_api_addr.startswith('http://'):
                metric_api_addr = 'http://' + metric_api_addr 
            
            logger.info("Test 1: Checking status of 3 node exporter")
            
            # Sleep & retry is need to for kubelet to setup networking
            #  It takes about 2 minutes for the network to get back on track
            #  for the frontend-exporter to expose metrics correct. 
            retry_count = MAX_RETRY
            while retry_count:
                try:
                    check_target_health(metric_api_addr)
                    retry_count = 0
                except AssertionError as e:
                    print("Exception noted. Will retry again in 60 seconds.")
                    print(e)
                    retry_count -= 1
                    if retry_count == 0: # a.k.a. the last retry
                        raise e
                    else:
                        time.sleep(69)
                        pass # try again. 

            logger.info("Test 1 Passed")

            logger.info("Test 2: Checking Model Container Metrics")
            conf = get_metrics_config()
            conf = conf['Model Container']
            prefix = 'clipper_{}_'.format(conf.pop('prefix'))
            for name, spec in conf.items():
                name = prefix + name
                if spec['type'] == 'Histogram' or spec['type'] == 'Summary':
                    name += '_sum'
                res = get_matched_query(metric_api_addr, name)
                parse_res_and_assert_node(res, node_num=2)
            logger.info("Test 2 Passed")
            # End Metric Check
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
            create_kubernetes_connection(cleanup=True, start_clipper=False)
            sys.exit(1)
        except ClipperException as e:
            log_clipper_state(clipper_conn)
            logger.exception("ClipperException")
            create_kubernetes_connection(cleanup=True, start_clipper=False)
            sys.exit(1)
    except Exception as e:
        logger.exception("Exception: {}".format(e))
        sys.exit(1)
