from __future__ import print_function
import sys
import os
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.abspath('%s/../../management/' % cur_dir))
import clipper_manager as cm
import json
import requests
from datetime import datetime
import time
import numpy as np


def predict(host, uid, x):
    url = "http://%s:1337/example_app/predict" % host
    req_json = json.dumps({'uid': uid, 'input': list(x)})
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


if __name__ == '__main__':
    host = "localhost"
    clipper = cm.Clipper(host, check_for_docker=False)
    clipper.register_application("example_app", "example_model", "doubles",
                                 "-1.0", 40000)
    clipper.register_external_model("example_model", 1, ["l1", "l2"],
                                    "doubles")
    time.sleep(1.0)
    uid = 0
    while True:
        predict(host, uid, np.random.random(200))
        time.sleep(0.2)
