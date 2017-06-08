from __future__ import print_function
import sys
import os
from clipper_admin import Clipper
import json
import requests
from datetime import datetime
import time
import numpy as np


def predict(host, x):
    url = "http://%s:1337/example_app/predict" % host
    req_json = json.dumps({'input': list(x)})
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


if __name__ == '__main__':
    host = "localhost"
    clipper = Clipper(host, check_for_docker=False)
    clipper.register_application("example_app", "doubles", "-1.0", 40000)
    clipper.register_external_model("example_model", 1, "doubles")
    time.sleep(1.0)
    clipper.link_model_to_app("example_app", "example_model")
    time.sleep(1.0)
    while True:
        predict(host, np.random.random(200))
        time.sleep(0.2)
