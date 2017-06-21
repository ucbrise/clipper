from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/.." % cur_dir))
from clipper_admin import Clipper
import time
import subprocess32 as subprocess
import pprint
import random
import socket

from pandas import *
from rpy2.robjects.packages import importr
import rpy2.robjects as ro
from rpy2.robjects import r, pandas2ri
pandas2ri.activate()
stats = importr('stats')
base = importr('base')

headers = {'Content-type': 'application/json'}
app_name = "R_model_test"
model_name = "R_model"

import sys
if sys.version_info[0] < 3:
    from StringIO import StringIO
else:
    from io import StringIO


class BenchmarkException(Exception):
    def __init__(self, value):
        self.parameter = value

    def __str__(self):
        return repr(self.parameter)


# range of ports where available ports can be found
PORT_RANGE = [34256, 40000]


def find_unbound_port():
    """
    Returns an unbound port number on 127.0.0.1.
    """
    while True:
        port = random.randint(*PORT_RANGE)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.bind(("127.0.0.1", port))
            return port
        except socket.error:
            print("randomly generated port %d is bound. Trying again." % port)


def init_clipper():
    clipper = Clipper("localhost", redis_port=find_unbound_port())
    clipper.stop_all()
    clipper.start()
    time.sleep(1)
    return clipper


def train_R_model():
    return ro.r('model_R <- lm(mpg~wt+cyl,data=train_data)')


def call_predictions(query_string):
    default = 0
    url = "http://localhost:1337/%s/predict" % app_name
    req_json = json.dumps({'uid': 0, 'input': query_string})
    response = requests.post(url, headers=headers, data=req_json)
    result = response.json()
    x = pandas.read_csv(StringIO(result["output"]), sep=";", index_col=0)
    print(x)

    if response.status_code == requests.codes.ok and result["default"] == True:
        default = 1
    elif response.status_code != requests.codes.ok:
        print(result)
        raise BenchmarkException(response.text)
    return default


def predict_R_model(df):
    #This function encodes pandas dataframe to strings,preserving its schema.
    #Moreover it calls for the actual predictions by invoking call_predictions() method.  
    num_defaults = 0
    query_string = df.to_csv(sep=";")
    num_defaults = call_predictions(query_string)
    return num_defaults


def deploy_and_test_model(clipper, model, version, test_data_collection):
    #test_data_collection is dictionary of pandas dataframes.
    clipper.deploy_R_model(model_name, version, model,
                           "clipper/r_python_container:latest", "string")
    time.sleep(25)
    num_defaults = 0
    num_preds = len(test_data_collection)
    print(num_preds)
    for i in range(0,num_preds):
        num_defaults += predict_R_model(test_data_collection[i])

    if num_defaults > 0:
        print("Error: %d/%d predictions were default" % (num_defaults,
                                                         num_preds))
    if num_defaults > num_preds / 2:
        raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                 (app_name, model_name, version))


if __name__ == "__main__":
    pos_label = 3
    try:
        clipper = init_clipper()

        #preparing datasets for training and testing 
        #using dataset mtcars , already provided by R. It has 32 rows and various coloums for eg. mpg,wt,cyl etc  
        #splitting it for training and testing in ratio 1:1. Further splitting the test data in ratio 1:1
        train_data = ro.r('train_data=head(mtcars,0.5*nrow(mtcars))')
        test_data = ro.r('test_data=tail(mtcars,0.5*nrow(mtcars))')
        test_data_collection={}
        test_data_collection[0]=ro.r('test_data1=head(test_data,0.5*nrow(test_data))')
        test_data_collection[1]=ro.r('test_data2=tail(test_data,0.5*nrow(test_data))')

        try:
            clipper.register_application(app_name, model_name, "string",
                                         "default_pred", 100000000)
            time.sleep(1)
            response = requests.post(
                "http://localhost:1337/%s/predict" % app_name,
                headers=headers,
                data=json.dumps({
                    'uid': 0,
                    'input': ""
                }))
            result = response.json()
            if response.status_code != requests.codes.ok:
                print("Error: %s" % response.text)
                raise BenchmarkException("Error creating app %s" % app_name)

            version = 1
            R_model = train_R_model()
            deploy_and_test_model(clipper, R_model, version, test_data_collection)
        except BenchmarkException as e:
            print(e)
            clipper.stop_all()
            sys.exit(1)
        else:
            clipper.stop_all()
            print("ALL TESTS PASSED")
    except Exception as e:
        print(e)
        clipper = Clipper("localhost")
        clipper.stop_all()
        sys.exit(1)
