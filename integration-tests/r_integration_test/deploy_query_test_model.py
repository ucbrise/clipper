from __future__ import absolute_import, division, print_function
import sys
import os
import time
import requests
import json
import logging
import numpy as np
if sys.version_info < (3, 0):
    import subprocess32 as subprocess
else:
    import subprocess

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../../clipper_admin" % cur_dir))

sys.path.insert(0, os.path.abspath("%s/.." % cur_dir))
from test_utils import create_docker_connection, headers, BenchmarkException

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

APP_NAME = "rtest-app"
APP_DEFAULT_VALUE = "NONE"
APP_SLO = 1000000

INPUT_TYPE = "doubles"

MODEL_NAME = "rtest-model"
MODEL_VERSION = 1
MODEL_IMAGE_NAME = "default-cluster-rtest-model:1"


def create_application(clipper_conn):
    clipper_conn.register_application(APP_NAME, INPUT_TYPE, APP_DEFAULT_VALUE,
                                      APP_SLO)
    time.sleep(1)


def deploy_and_link_model(clipper_conn):
    subprocess.check_call(["Rscript", "build_test_model.R"])
    clipper_conn.deploy_model(MODEL_NAME, MODEL_VERSION, INPUT_TYPE,
                              MODEL_IMAGE_NAME)
    clipper_conn.link_model_to_app(app_name=APP_NAME, model_name=MODEL_NAME)


def send_requests(clipper_conn):
    success = False
    num_tries = 0

    while not success and num_tries < 5:
        time.sleep(30)
        num_preds = 25
        num_success = 0
        addr = clipper_conn.get_query_addr()
        logger.info("ADDR: {}".format(addr))
        for i in range(num_preds):
            response = requests.post(
                "http://%s/%s/predict" % (addr, APP_NAME),
                headers=headers,
                data=json.dumps({
                    'input': list(np.random.random(30))
                }))
            result = response.json()
            if response.status_code == requests.codes.ok and not result["default"]:
                num_success += 1
            else:
                logger.warning(result)
        if num_success < num_preds:
            logger.error(
                "Error: %d/%d predictions were default or unsuccessful" %
                (num_preds - num_success, num_preds))
        if num_success > num_preds / 2.0:
            success = True
        num_tries += 1

    if not success:
        raise BenchmarkException("Error querying R model")


if __name__ == "__main__":

    try:
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)
        time.sleep(10)
        try:
            logger.info("Running R model deployment test")
            create_application(clipper_conn)
            deploy_and_link_model(clipper_conn)
            time.sleep(5)
            send_requests(clipper_conn)
            logger.info("R model deployment completed SUCCESSFULLY!")
        except BenchmarkException as e:
            logger.exception("BenchmarkException in R model deployment test")
            create_docker_connection(cleanup=True, start_clipper=False)
            sys.exit(1)
        else:
            create_docker_connection(cleanup=True, start_clipper=False)
    except Exception as e:
        logger.exception("Exception")
        create_docker_connection(cleanup=True, start_clipper=False)
        sys.exit(1)
