from __future__ import print_function
import sys
import json
import os
import requests
import random
from datetime import datetime
import time

import pandas as pd
import numpy as np
import sklearn.linear_model

def load_digits(digits_location, digits_filename = "train-mnist-dense-with-labels.data"):
    digits_path = digits_location + "/" + digits_filename
    print("Source file:", digits_path)
    df = pd.read_csv(digits_path, sep=",", header=None)
    data = df.values
    print("Number of image files:", len(data))
    y = data[:,0]
    X = data[:,1:]
    return (X, y)

def normalize_digits(X):
    mu = np.mean(X,0)
    sigma = np.var(X,0)
    Z = (X - mu) / np.array([np.sqrt(z) if z > 0 else 1. for z in sigma])
    return Z 

def mnist_update(uid, x, y):
    url = "http://localhost:1337/update"
    req_json = json.dumps({
        'uid': uid,
        'input': list(x),
        'y': y,
        'model': {
            'name': 'm',
            'version': 1
            }
        })
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))

def mnist_prediction(host, uid, x):
    url = "http://%s:1337/predict" % host
    req_json = json.dumps({'uid': uid, 'input': list(x)})
    headers = {'Content-type': 'application/json'}
    # x_str = ", ".join(["%d" % a for a in x])
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))

if __name__=='__main__':
    args = sys.argv
    x, y = load_digits(os.path.expanduser("~/model-serving/data/mnist_data"), digits_filename = "test.data")
    # z = normalize_digits(x)
    # inputs = args[2:]
    # uid = int(args[1])
    uid = 4
    # num_inputs = int(args[2])
    while True:
        # mnist_update(uid, x[int(i)], float(y[int(i)]))
        example_num = np.random.randint(0,len(x))
        # mnist_update(uid, x[example_num], float(y[example_num]))
        mnist_prediction("localhost", uid, x[example_num])
        time.sleep(0.2)


