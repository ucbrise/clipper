from clipper_admin import ClipperConnection, DockerContainerManager, KubernetesContainerManager
from clipper_admin.deployers import python as python_deployer

import signal
import sys
import json
import requests
from datetime import datetime
import os
import time
from test_utils import create_kubernetes_connection, create_docker_connection, CLIPPER_CONTAINER_REGISTRY
from random import randint


def test(kubernetes):
    conn_1 = create('multi-tenancy-1-{}'.format(randint(1,9999)), use_kubernetes=kubernetes)
    conn_2 = create('multi-tenancy-2-{}'.format(randint(1,9999)), use_kubernetes=kubernetes)

    deploy_(conn_1, use_kubernetes=kubernetes)
    deploy_(conn_2, use_kubernetes=kubernetes)

    time.sleep(10)

    res_1 = predict_(conn_1.get_query_addr(), [.1, .2, .3])
    res_2 = predict_(conn_2.get_query_addr(), [.1, .2, .3])
    assert not res_1['default']
    assert not res_2['default']

    conn_1.stop_all()
    conn_2.stop_all()


def create(name, use_kubernetes=False):
    if use_kubernetes:
        conn = create_kubernetes_connection(
            cleanup=False, start_clipper=True, new_name=name)
    else:
        conn = create_docker_connection(
            cleanup=False, start_clipper=True, new_name=name)
    return conn


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


def deploy_(clipper_conn, use_kubernetes=False):
    if use_kubernetes:
        python_deployer.create_endpoint(
            clipper_conn,
            "testapp0-model",
            "doubles",
            feature_sum,
            registry=CLIPPER_CONTAINER_REGISTRY)
    else:
        python_deployer.create_endpoint(clipper_conn, "testapp0-model",
                                        "doubles", feature_sum)


def predict_(addr, x, batch=False):
    url = "http://%s/testapp0-model/predict" % addr

    if batch:
        req_json = json.dumps({'input_batch': x})
    else:
        req_json = json.dumps({'input': list(x)})

    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))
    return r.json()


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--kubernetes':
        test(kubernetes=True)
    else:
        test(kubernetes=False)
