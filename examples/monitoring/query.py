from __future__ import print_function
import sys
sys.path.insert(0, '../../clipper_admin')
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys


def predict(addr, x, batch=False):
    url = "http://%s/simple-example/predict" % addr

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
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.stop_all()
    sys.exit(0)


def produce_query_arr_for_ms(ms):
    size = int(ms * 8000)  ## Use python sum, scale linearly.
    return np.random.random(size)


def fizz_buzz(i):
    if i % 15 == 0:
        return produce_query_arr_for_ms(200)
    elif i % 5 == 0:
        return produce_query_arr_for_ms(100)
    elif i % 3 == 0:
        return produce_query_arr_for_ms(50)
    else:
        return produce_query_arr_for_ms(10)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal_handler)
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.start_clipper()
    print("Starting Clipper")
    python_deployer.create_endpoint(
        clipper_conn, "simple-example", "doubles", feature_sum, num_replicas=2)
    time.sleep(2)
    print("Starting Prediction")

    try:
        counter = 0
        while True:
            print(counter)
            predict(clipper_conn.get_query_addr(), fizz_buzz(counter))
            counter += 1
            time.sleep(0.2)
    except Exception as e:
        clipper_conn.stop_all()
