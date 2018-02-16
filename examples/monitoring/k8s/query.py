from __future__ import print_function
import sys
sys.path.insert(0, '../../../clipper_admin')
from clipper_admin import ClipperConnection, KubernetesContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys

KUBE_API_IP = None
assert KUBE_API_IP, "KUBE_API_IP Missing"


def predict(addr, x, batch=False):
    url = "http://%s/simple-noop-app/predict" % addr

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


def feature_sum(xs):
    return [str(sum(x)) for x in xs]


# Stop Clipper on Ctrl-C
def signal_handler(signal, frame):
    print("Stopping Clipper...")
    clipper_conn = ClipperConnection(KubernetesContainerManager(KUBE_API_IP))
    clipper_conn.stop_all()
    sys.exit(0)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal_handler)
    clipper_conn = ClipperConnection(KubernetesContainerManager(KUBE_API_IP))
    clipper_conn.start_clipper()
    print("Starting Clipper")
    clipper_conn.register_application("simple-noop-app", "doubles",
                                      "default_pred", 100000)
    clipper_conn.deploy_model(
        name='simple-fizz-buzz-model',
        version='1',
        input_type='doubles',
        image='simonmok/fizz-buzz:latest',
        num_replicas=2)
    clipper_conn.link_model_to_app("simple-noop-app", 'simple-fizz-buzz-model')
    time.sleep(2)
    print("Starting Prediction")
    print("Query Address: {}".format(clipper_conn.get_query_addr()))
    try:
        counter = 0
        while True:
            print(counter)
            predict(clipper_conn.get_query_addr(), np.random.randn(100))
            counter += 1
            time.sleep(2)
    except Exception as e:
        print(e)
