from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

cur_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.abspath('%s/util_direct_import/' % cur_dir))
from util_package import mock_module_in_package as mmip
import mock_module as mm

import tensorflow as tf
import argparse

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin.deployers.tensorflow import deploy_tensorflow_model, create_endpoint

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "tensorflow-test"
model_name = "tensorflow-model"


def normalize(x):
    return x.astype(np.double) / 255.0


def objective(y, pos_label):
    # prediction objective
    if y == pos_label:
        return 1
    else:
        return 0


def parseData(train_path, pos_label):
    trainData = np.genfromtxt(train_path, delimiter=',', dtype=int)
    records = trainData[:, 1:]
    labels = trainData[:, :1]
    transformedlabels = [objective(ele, pos_label) for ele in labels]
    return (records, transformedlabels)


def reset_vars(sess):
    sess.run(tf.global_variables_initializer())


def reset_tf(sess):
    if sess:
        sess.close()
    tf.reset_default_graph()
    sess = tf.Session()
    return sess


def predict(sess, inputs):
    preds = sess.run('predict_class:0', feed_dict={'pixels:0': inputs})
    return [str(p) for p in preds]


def deploy_and_test_model(clipper_conn,
                          sess,
                          version,
                          input_type,
                          link_model=False,
                          predict_fn=predict):
    deploy_tensorflow_model(clipper_conn, model_name, version, input_type,
                            predict_fn, sess)

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


def train_logistic_regression(sess, X_train, y_train):
    sess = reset_tf(sess)
    x = tf.placeholder(tf.float32, [None, X_train.shape[1]], name="pixels")
    y_labels = tf.placeholder(tf.int32, [None], name="labels")
    y = tf.one_hot(y_labels, depth=2)

    W = tf.Variable(tf.zeros([X_train.shape[1], 2]), name="weights")
    b = tf.Variable(tf.zeros([2]), name="biases")
    y_hat = tf.matmul(x, W) + b

    pred = tf.argmax(tf.nn.softmax(y_hat), 1, name="predict_class")  # Softmax

    loss = tf.reduce_mean(
        tf.nn.softmax_cross_entropy_with_logits(logits=y_hat, labels=y))
    train = tf.train.GradientDescentOptimizer(0.1).minimize(loss)

    accuracy = tf.reduce_mean(
        tf.cast(tf.equal(tf.argmax(y_hat, 1), tf.argmax(y, 1)), tf.float32))
    reset_vars(sess)
    for i in range(5000):
        sess.run(train, feed_dict={x: X_train, y_labels: y_train})
        if i % 1000 == 0:
            print('Cost , Accuracy')
            print(sess.run(
                [loss, accuracy], feed_dict={
                    x: X_train,
                    y_labels: y_train
                }))
    return sess


def get_test_point():
    return [np.random.randint(255) for _ in range(784)]


if __name__ == "__main__":
    pos_label = 3
    try:
        sess = None
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)

        train_path = os.path.join(cur_dir, "data/train.data")
        (X_train, y_train) = parseData(train_path, pos_label)

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

            sess = train_logistic_regression(sess, X_train, y_train)
            #  Save the TF Model .. the saved model is used for subsequent tests of serving saved models
            saver = tf.train.Saver()
            save_path = saver.save(sess, "data/model.ckpt")

            # Deploy a TF model using the Tensorflow Session
            version = 1
            deploy_and_test_model(
                clipper_conn, sess, version, "integers", link_model=True)

            # Deploy a TF Model using a saved Tensorflow Model
            version += 1
            deploy_and_test_model(
                clipper_conn, "data", version, "integers", link_model=False)

            # Freeze the Graph and save it
            output_graph_dir = "data" + "/frozen_model.pb"
            output_graph_file = "frozen_model.pb"
            output_node_names = "predict_class"
            graph = tf.get_default_graph()
            input_graph_def = graph.as_graph_def()
            output_graph_def = tf.graph_util.convert_variables_to_constants(
                sess,  # The session is used to retrieve the weights
                input_graph_def,  # The graph_def is used to retrieve the nodes
                output_node_names.split(
                    ","
                )  # The output node names are used to select the usefull nodes
            )
            # Finally we serialize and dump the output graph to the filesystem
            with tf.gfile.GFile(output_graph_dir, "wb") as f:
                f.write(output_graph_def.SerializeToString())
            print("%d ops in the final graph." % len(output_graph_def.node))

            with tf.gfile.GFile(output_graph_file, "wb") as f:
                f.write(output_graph_def.SerializeToString())
            print("%d ops in the final graph." % len(output_graph_def.node))
            sess.close()

            # Deploy a TF Model using a Frozen Tensorflow Model in a directory
            version += 1
            deploy_and_test_model(
                clipper_conn, "data", version, "integers", link_model=False)

            # Deploy a TF Model using a Frozen Tensorflow Model in a file
            version += 1
            deploy_and_test_model(
                clipper_conn,
                "frozen_model.pb",
                version,
                "integers",
                link_model=False)

            app_and_model_name = "easy-register-app-model"
            create_endpoint(clipper_conn, app_and_model_name, "integers",
                            predict, "data")
            test_model(clipper_conn, app_and_model_name, 1)

        except BenchmarkException as e:
            log_clipper_state(clipper_conn)
            logger.exception("BenchmarkException")
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False)
            sys.exit(1)
        else:
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False)
    except Exception as e:
        logger.exception("Exception")
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False)
        sys.exit(1)
