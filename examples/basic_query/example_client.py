from __future__ import print_function
import json
import requests
from datetime import datetime
import time
import numpy as np


def update(host, uid, x, y):
    url = "http://%s:1337/example_app/update" % host
    req_json = json.dumps({
        'uid': uid,
        'input': list(x),
        'label': float(y),
        'model_name': 'example_model',
        'model_version': 1
        })
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


def predict(host, uid, x):
    url = "http://%s:1337/example_app/predict" % host
    req_json = json.dumps({'uid': uid, 'input': list(x)})
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


def add_mnist_app(host):
    url = "http://%s:1338/admin/add_app" % host
    req_json = json.dumps({
     "name": "example_app",
     "candidate_models": [{"model_name": "example_model", "model_version": 1}],
     "input_type": "doubles",
     "output_type": "double",
     "selection_policy": "simple_policy",
     "latency_slo_micros": 20000
    })
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))

if __name__ == '__main__':
    add_mnist_app("localhost")
    time.sleep(1.0)
    uid = 4
    while True:
        # mnist_update(uid, x[example_num], float(y[example_num]))
        predict("localhost", uid, np.random.random(1000))
        time.sleep(0.2)
