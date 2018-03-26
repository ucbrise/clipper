from __future__ import print_function
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys


def predict(x, batch=False):
    url = "http://localhost:1337/app1/predict"

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
    # clipper_conn = ClipperConnection(DockerContainerManager())
    # clipper_conn.stop_all()
    sys.exit(0)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal_handler)
    # clipper_conn = ClipperConnection(DockerContainerManager())
    # clipper_conn.start_clipper()
    # python_deployer.create_endpoint(clipper_conn, "simple-example", "doubles",
    #                                 feature_sum)
    time.sleep(2)

    # For batch inputs set this number > u1
    batch_size = 1

    try:
        while True:
            if batch_size > 1:
                predict(
                    [list(np.random.random(200)) for i in range(batch_size)],
                    batch=True)
            else:
                predict(np.random.random(200))
            time.sleep(0.2)
    except Exception as e:
        clipper_conn.stop_all()
