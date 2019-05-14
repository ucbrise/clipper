from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging
import random

cur_dir = os.path.dirname(os.path.abspath(__file__))

import mxnet as mx

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state, log_docker, log_cluster_model)

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))

from clipper_admin.deployers.mxnet import deploy_mxnet_model, create_endpoint

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "mxnet-test"
model_name = "mxnet-model"


def predict(model, xs):
    data_iter = mx.io.NDArrayIter(xs)
    preds = model.predict(data_iter)
    preds = [preds[0]]
    return [str(p) for p in preds]


def deploy_and_test_model(clipper_conn,
                          model,
                          data_shapes,
                          version,
                          link_model=False,
                          predict_fn=predict):
    deploy_mxnet_model(
        clipper_conn,
        model_name,
        version,
        "integers",
        predict_fn,
        model,
        data_shapes,
        batch_size=1)

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


def get_test_point():
    return [np.random.randint(255) for _ in range(785)]


if __name__ == "__main__":
    pos_label = 3

    import random
    cluster_name = "mxnet-{}".format(random.randint(0, 5000))
    try:
        clipper_conn = create_docker_connection(
            new_name=cluster_name, cleanup=False, start_clipper=True)

        train_path = os.path.join(cur_dir, "data/train.data")
        data_iter = mx.io.CSVIter(
            data_csv=train_path, data_shape=(785, ), batch_size=1)

        try:
            clipper_conn.register_application(app_name, "integers",
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

            version = 1

            # Create a MXNet model
            # Configure a two layer neuralnetwork
            data = mx.symbol.Variable('data')
            fc1 = mx.symbol.FullyConnected(data, name='fc1', num_hidden=128)
            act1 = mx.symbol.Activation(fc1, name='relu1', act_type='relu')
            fc2 = mx.symbol.FullyConnected(act1, name='fc2', num_hidden=10)
            softmax = mx.symbol.SoftmaxOutput(fc2, name='softmax')

            # Initialize the module and fit it
            mxnet_model = mx.mod.Module(softmax)
            mxnet_model.fit(data_iter, num_epoch=0)

            train_data_shape = data_iter.provide_data

            deploy_and_test_model(
                clipper_conn,
                mxnet_model,
                train_data_shape,
                version,
                link_model=True)
            app_and_model_name = "easy-register-app-model"
            create_endpoint(
                clipper_conn,
                app_and_model_name,
                "integers",
                predict,
                mxnet_model,
                train_data_shape,
                batch_size=1)
            test_model(clipper_conn, app_and_model_name, 1)

        except BenchmarkException:
            logger.exception("BenchmarkException")
            log_docker(clipper_conn)
            log_cluster_model(clipper_conn, cluster_name)
            log_clipper_state(clipper_conn)
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
            sys.exit(1)
        else:
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
    except Exception:
        logger.exception("Exception")
        log_docker(clipper_conn)
        log_cluster_model(clipper_conn, cluster_name)
        log_clipper_state(clipper_conn)
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=cluster_name)

        sys.exit(1)
