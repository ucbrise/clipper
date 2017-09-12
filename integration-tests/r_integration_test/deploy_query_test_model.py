from __future__ import absolute_import, division, print_function
import sys
import os
import time
import requests
import json
import logging
import numpy as np

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../../clipper_admin" % cur_dir))

from clipper_admin import ClipperConnection, DockerContainerManager
from ..test_utils import create_docker_connection, headers, BenchmarkException

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

APP_NAME = "rtest_app"
APP_DEFAULT_VALUE = "default"
APP_SLO = 10000000

INPUT_TYPE = "doubles"

MODEL_NAME = "rtest-model"
MODEL_VERSION = 1
MODEL_IMAGE_NAME = "rtest-model:1"


def create_application(conn):
    conn.register_application(APP_NAME, INPUT_TYPE, APP_DEFAULT_VALUE, APP_SLO)


def deploy_and_link_model(conn):
    conn.deploy_model(MODEL_NAME, MODEL_VERSION, INPUT_TYPE, MODEL_IMAGE_NAME)
    conn.link_model_to_app(APP_NAME, MODEL_NAME)


def send_requests(conn):

    success = False
    num_tries = 0

    while not success and num_tries < 5:
        time.sleep(30)
        num_preds = 25
        num_success = 0
        addr = conn.get_query_addr()
        for i in range(num_preds):
            response = requests.post(
                "http://%s/%s/predict" % (addr, APP_NAME),
                headers=headers,
                data=json.dumps({
                    'input': list(np.random.random(i))
                }))
            result = response.json()
            if response.status_code == requests.codes.ok and not result["default"]:
                num_success += 1
            if num_success != num_preds > 0:
                logger.error(
                    "Error: %d/%d predictions were default or unsuccessful" %
                    (num_preds - num_success, num_preds))
        if num_success > num_preds / 2.0:
            success = True
        num_tries += 1

    if not success:
        raise BenchmarkException("Error querying R model")


if __name__ == "__main__":

    conn = create_docker_connection(cleanup=True, start_clipper=True)

    create_application(conn)
    deploy_and_link_model(conn)

    time.sleep(5)

    send_requests(conn)
    conn.stop_all()

    try:
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)
        time.sleep(10)
        try:
            logger.info("Running R model deployment test")
            create_application(conn)
            deploy_and_link_model(conn)
            time.sleep(5)
            send_requests(conn)
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
