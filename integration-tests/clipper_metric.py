from __future__ import print_function
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys
import os
import logging
from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state)

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


logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

if __name__ == '__main__':
    logger.info("Start Metric Test (0/1): Running 2 Replicas")
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.start_clipper()
    python_deployer.create_endpoint(clipper_conn, "simple-example", "doubles",
                                    feature_sum, num_replicas=2)
    time.sleep(2)
    try:
        for _ in range(100):
            predict(clipper_conn.get_query_addr(), np.random.random(200))
            time.sleep(0.2)
        up_response = requests.get("http://localhost:9090/api/v1/series?match[]=up").json()
        logger.debug(up_response)
        assert up_response['status'] == 'success'
        assert len(up_response['data']) == 3
        logger.info("Metric Test Done, Cleaning up...")
        clipper_conn.stop_all()
    except Exception as e:
        logger.error(e)
        log_clipper_state(clipper_conn)
        clipper_conn.stop_all()
        sys.exit(1)
