from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

cur_dir = os.path.dirname(os.path.abspath(__file__))

from pyspark.ml.linalg import Vectors
from pyspark.sql import SparkSession, Row
from pyspark.ml.classification import LogisticRegression

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state, log_docker)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin.deployers.pyspark import deploy_pyspark_model

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "pyspark-sparkml-test"
model_name = "pyspark-sparkml-model"


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
    return (obj(int(fields[0]), pos_label),
            Vectors.dense(normalize(np.array(fields[1:]))))


def predict(spark, model, xs):
    df = spark.sparkContext.parallelize(
        [Row(features=Vectors.dense(x)) for x in xs]).toDF()
    result = model.transform(df).select('prediction').collect()
    return ([str(x[0]) for x in result])


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
        print(result)
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


def train_logistic_regression(trainDF):
    mlr = LogisticRegression(maxIter=100, regParam=0.03)
    mlrModel = mlr.fit(trainDF)
    return mlrModel


def get_test_point():
    return [np.random.randint(255) for _ in range(784)]


if __name__ == "__main__":
    pos_label = 3

    import random
    cluster_name = "sparkml-{}".format(random.randint(0, 5000))
    clipper_conn = None

    try:
        spark = SparkSession\
                .builder\
                .appName("clipper-pyspark-ml")\
                .config("spark.ui.enabled", "false")\
                .getOrCreate()
        sc = spark.sparkContext
        clipper_conn = create_docker_connection(
            cleanup=False, start_clipper=True, new_name=cluster_name)

        train_path = os.path.join(cur_dir, "data/train.data")
        trainRDD = spark.sparkContext.textFile(train_path).map(
            lambda line: parseData(line, objective, pos_label)).cache()
        trainDf = spark.createDataFrame(trainRDD, ["label", "features"])

        try:
            clipper_conn.register_application(app_name, "integers",
                                              "default_pred", 2000000)
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
            lr_model = train_logistic_regression(trainDf)
            deploy_and_test_model(
                sc, clipper_conn, lr_model, version, link_model=True)
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
