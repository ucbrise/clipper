from __future__ import print_function
import json
import requests
from datetime import datetime
import time
import numpy as np
import sys


def predict(host, uid, x):
    url = "http://%s:1337/example_app/predict" % host
    req_json = json.dumps({'uid': uid, 'input': list(x)})
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


def add_example_app(host):
    url = "http://%s:1338/admin/add_app" % host
    req_json = json.dumps({
        "name": "example_app",
        "candidate_model_names": ["example_model"],
        "input_type": "doubles",
        "default_output": -1.0,
        "latency_slo_micros": 20000
    })
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)
    if r.status_code != requests.codes.ok:
        print(r.text)
        sys.exit(1)


def add_example_model(host):
    url = "http://%s:1338/admin/add_model" % host
    req_json = json.dumps({
        "model_name": "example_model",
        "model_version": 1,
        "labels": ["l1", "l2"],
        "input_type": "doubles",
        "container_name": "EXTERNAL",
        "model_data_path": "EXTERNAL"
    })
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)
    if r.status_code != requests.codes.ok:
        print(r.text)
        sys.exit(1)


if __name__ == '__main__':
    host = "localhost"
    add_example_app(host)
    add_example_model(host)
    time.sleep(1.0)
    uid = 0
    while True:
        predict(host, uid, np.random.random(200))
        time.sleep(0.2)
