from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

cur_dir = os.path.dirname(os.path.abspath(__file__))

import keras

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state)

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))

import clipper_admin.deployers.keras as keras_deployer

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "keras-test"
model_name = "keras-model"


def predict(model, inputs):
    return [model.predict(x) for x in inputs]


def deploy_and_test_model(clipper_conn,
                          model,
                          version,
                          input_type,
                          link_model=False,
                          predict_fn=predict):
    keras_deployer.deploy_keras_model(clipper_conn=clipper_conn,
                                      name=model_name,
                                      version=version,
                                      input_type=input_type,
                                      func=predict_fn,
                                      model_path_or_object=model)
    time.sleep(5)

    if link_model:
        clipper_conn.link_model_to_app(app_name, model_name)
        time.sleep(5)

    test_model(clipper_conn, app_name, version)


def test_model(clipper_conn, app, version):
    time.sleep(25)
    num_preds = 25
    num_defaults = 0
    addr = clipper_conn.get_query_addr()
    print(addr)
    for i in range(num_preds):
        response = requests.post(
            "http://%s/%s/predict" % (addr, app),
            headers=headers,
            data=json.dumps({
                'input': get_test_point()
            }))
        result = response.json()
        if response.status_code == requests.codes.ok and result["default"]:
            num_defaults += 1
        elif response.status_code != requests.codes.ok:
            print(result)
            raise BenchmarkException(response.text)

    if num_defaults > 0:
        print("Error: %d/%d predictions were default" % (num_defaults,
                                                         num_preds))
    if num_defaults > num_preds / 2:
        raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                 (app, model_name, version))


def create_simple_keras_model():
    inpt = keras.layers.Input(shape=(1,))
    out = keras.layers.multiply([inpt, inpt])
    model = keras.models.Model(inputs=inpt, outputs=out)
    return model


def get_test_point():
    return [np.random.randint(20) for _ in range(100)]


if __name__ == "__main__":

    import random

    cluster_name = "keras-{}".format(random.randint(0, 5000))
    try:
        model = create_simple_keras_model()
        model_path = os.path.join(cur_dir, "data/model.h5")
        model.save(model_path)

        clipper_conn = create_docker_connection(
            cleanup=False, start_clipper=True, new_name=cluster_name)

        try:
            clipper_conn.register_application(app_name, "ints",
                                              "default_pred", 100000)
            time.sleep(1)

            addr = clipper_conn.get_query_addr()
            response = requests.post(
                "http://%s/%s/predict" % (addr, app_name),
                headers=headers,
                data=json.dumps({
                    'input': get_test_point()
                }))
            result = response.json()
            if response.status_code != requests.codes.ok:
                print("Error: %s" % response.text)
                raise BenchmarkException("Error creating app %s" % app_name)

            # Deploy a Keras model using the Model Object
            version = 1
            deploy_and_test_model(
                clipper_conn, model, version, "ints", link_model=True)

            # Deploy a Keras Model using a saved Keras Model ('.h5')
            version += 1
            deploy_and_test_model(
                clipper_conn, model_path, version, "ints", link_model=False)

        except BenchmarkException:
            log_clipper_state(clipper_conn)
            logger.exception("BenchmarkException")
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
            sys.exit(1)
        else:
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
    except Exception:
        logger.exception("Exception")
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=cluster_name)
        sys.exit(1)
