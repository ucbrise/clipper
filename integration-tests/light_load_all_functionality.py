from __future__ import print_function
import os
import sys
import requests
import json
import numpy as np
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath('%s/../management/' % cur_dir))
import clipper_manager as cm
import time
import subprocess32 as subprocess
import pprint
import random
import socket

headers = {'Content-type': 'application/json'}
fake_model_data = "/tmp/test123456"
try:
    os.mkdir(fake_model_data)
except OSError:
    pass


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
    port = random.randint(*PORT_RANGE)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind(("127.0.0.1", port))
    except socket.error:
        port = get_port()
    return port


def init_clipper():
    clipper = cm.Clipper("localhost", redis_port=find_unbound_port())
    clipper.start()
    time.sleep(1)
    return clipper


def print_clipper_state(clipper):
    pp = pprint.PrettyPrinter(indent=4)
    print("APPLICATIONS")
    pp.pprint(clipper.get_all_apps(verbose=True))
    print("\nMODELS")
    pp.pprint(clipper.get_all_models(verbose=True))
    print("\nCONTAINERS")
    pp.pprint(clipper.get_all_containers(verbose=True))


def deploy_model(clipper, name, version):
    app_name = "%s_app" % name
    model_name = "%s_model" % name
    clipper.deploy_model(
        model_name,
        version,
        fake_model_data,
        "clipper/noop-container", [name],
        "doubles",
        num_containers=1)
    time.sleep(20)
    num_preds = 25
    num_defaults = 0
    for i in range(num_preds):
        response = requests.post(
            "http://localhost:1337/%s/predict" % app_name,
            headers=headers,
            data=json.dumps({
                'uid': 0,
                'input': list(np.random.random(30))
            }))
        result = response.json()
        if response.status_code == requests.codes.ok and result["default"] == True:
            num_defaults += 1
    if num_defaults > 0:
        print("Error: %d/%d predictions were default" % (num_defaults,
                                                         num_preds))
    if num_defaults > num_preds / 2:
        raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                 (app_name, model_name, version))


def create_and_test_app(clipper, name):
    app_name = "%s_app" % name
    model_name = "%s_model" % name
    clipper.register_application(app_name, model_name, "doubles",
                                 "default_pred", 100000)
    time.sleep(1)
    response = requests.post(
        "http://localhost:1337/%s/predict" % app_name,
        headers=headers,
        data=json.dumps({
            'uid': 0,
            'input': list(np.random.random(30))
        }))
    result = response.json()
    if response.status_code != requests.codes.ok:
        print("Error: %s" % response.text)
        raise BenchmarkException("Error creating app %s" % app_name)

    for i in range(8):
        deploy_model(clipper, name, i)
        time.sleep(1)


def cleanup():
    # stop docker
    subprocess.call(
        "docker stop $(docker ps -aqf name=clipper_) && docker rm $(docker ps -aqf name=clipper_)",
        shell=True)

    # stop model containers
    subprocess.call(
        "docker stop $(docker ps -aqf ancestor=clipper/noop-container) && docker rm $(docker ps -aqf ancestor=clipper/noop-container)",
        shell=True)


if __name__ == "__main__":
    clipper = init_clipper()
    try:
        create_and_test_app(clipper, "aa")
        create_and_test_app(clipper, "bb")
        create_and_test_app(clipper, "cc")
        create_and_test_app(clipper, "dd")
        create_and_test_app(clipper, "ee")
        create_and_test_app(clipper, "ff")
        create_and_test_app(clipper, "gg")
        print("SUCCESS")
    except BenchmarkException as e:
        print_clipper_state(clipper)
        print(e)
        cleanup()
        sys.exit(1)
    else:
        cleanup()
