from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

cur_dir = os.path.dirname(os.path.abspath(__file__))

from pyspark.mllib.classification import LogisticRegressionWithSGD
from pyspark.mllib.classification import SVMWithSGD
from pyspark.mllib.tree import RandomForest
from pyspark.mllib.regression import LabeledPoint
from pyspark.sql import SparkSession

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state, log_docker)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin.deployers.pyspark import deploy_pyspark_model, create_endpoint

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "pyspark-test"
model_name = "pyspark-model"


def normalize(x):
    return x.astype(np.double) / 255.0


def objective(y, pos_label):
    # prediction objective
    if y == pos_label:
        return 1
    else:
        return 0


def parseData(line, obj, pos_label):
    fields = line.strip().split(',')
    return LabeledPoint(
        obj(int(fields[0]), pos_label), normalize(np.array(fields[1:])))


def predict(spark, model, xs):
    return [str(model.predict(normalize(x))) for x in xs]


def deploy_and_test_model(sc,
                          clipper_conn,
                          model,
                          version,
                          link_model=False,
                          predict_fn=predict):
    deploy_pyspark_model(clipper_conn, model_name, version, "integers",
                         predict_fn, model, sc)

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


def train_logistic_regression(trainRDD):
    return LogisticRegressionWithSGD.train(trainRDD, iterations=10)


def train_svm(trainRDD):
    return SVMWithSGD.train(trainRDD)


def train_random_forest(trainRDD, num_trees, max_depth):
    return RandomForest.trainClassifier(
        trainRDD, 2, {}, num_trees, maxDepth=max_depth)


def get_test_point():
    return [np.random.randint(255) for _ in range(784)]


if __name__ == "__main__":
    pos_label = 3

    import random
    cluster_name = "spark-{}".format(random.randint(0, 5000))
    clipper_conn = None

    try:
        spark = SparkSession\
                .builder\
                .appName("clipper-pyspark")\
                .config("spark.ui.enabled", "false")\
                .getOrCreate()
        sc = spark.sparkContext
        clipper_conn = create_docker_connection(
            cleanup=False, start_clipper=True, new_name=cluster_name)

        train_path = os.path.join(cur_dir, "data/train.data")
        trainRDD = sc.textFile(train_path).map(
            lambda line: parseData(line, objective, pos_label)).cache()

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
            lr_model = train_logistic_regression(trainRDD)
            deploy_and_test_model(
                sc,
                clipper_conn,
                lr_model,
                version,
                link_model=True,
                predict_fn=predict)

            version += 1
            svm_model = train_svm(trainRDD)
            deploy_and_test_model(sc, clipper_conn, svm_model, version)

            version += 1
            rf_model = train_random_forest(trainRDD, 20, 16)
            deploy_and_test_model(sc, clipper_conn, svm_model, version)

            app_and_model_name = "easy-register-app-model"
            create_endpoint(clipper_conn, app_and_model_name, "integers",
                            predict, lr_model, sc)
            test_model(clipper_conn, app_and_model_name, 1)

            version += 1
            deploy_and_test_model(
                sc, clipper_conn, lr_model, version, predict_fn=predict)
        except BenchmarkException as e:
            logger.exception("BenchmarkException: {}".format(e))
            log_docker(clipper_conn)
            log_clipper_state(clipper_conn)
            create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
            sys.exit(1)
        else:
            spark.stop()
            create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
    except Exception as e:
        logger.exception("Exception: {}".format(e))
        log_docker(clipper_conn)
        create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=cluster_name)
        sys.exit(1)
