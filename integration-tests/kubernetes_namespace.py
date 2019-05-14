"""
Sanity Check for Kubernetes Namespace(s)
"""

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


def test():
    conn_1 = create_kubernetes_connection(
        cleanup=False, start_clipper=True, namespace='ns-1')
    conn_2 = create_kubernetes_connection(
        cleanup=False, start_clipper=True, namespace='ns-2')

    deploy_(conn_1)
    deploy_(conn_2)

    time.sleep(10)

    res_1 = predict_(conn_1.get_query_addr(), [.1, .2, .3])
    res_2 = predict_(conn_2.get_query_addr(), [.1, .2, .3])
    assert not res_1['default']
    assert not res_2['default']

    conn_1.stop_all()
    conn_2.stop_all()


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


def deploy_(clipper_conn):
    python_deployer.create_endpoint(
        clipper_conn,
        "testapp0-model",
        "doubles",
        feature_sum,
        registry=CLIPPER_CONTAINER_REGISTRY)


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
    test()
