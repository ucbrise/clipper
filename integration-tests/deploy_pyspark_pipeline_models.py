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

import findspark
findspark.init()
import pyspark
from pyspark.ml import Pipeline
from pyspark.ml.classification import LogisticRegression
from pyspark.ml.feature import HashingTF, Tokenizer
from pyspark.sql import SparkSession

headers = {'Content-type': 'application/json'}
app_name = "pyspark_pipeline_test"
model_name = "pyspark_pipeline"


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


columns = ["id", "text"]


def json_to_dataframe(spark_session, xs):
    tuples = [tuple(json.loads(x)) for x in xs]
    df = spark_session.createDataFrame(tuples, columns)
    return df


def predict(spark, pipeline, xs):
    df = json_to_dataframe(spark, xs)
    preds = pipeline.transform(df)
    selected = preds.select("probability", "prediction")
    outputs = []
    for row in selected.collect():
        prob, prediction = row
        outputs.append(
            json.dumps({
                "prob": str(prob),
                "prediction": prediction
            }))
    return outputs


if __name__ == "__main__":
    spark = SparkSession\
        .builder\
        .appName("clipper-pyspark")\
        .getOrCreate()

    training = spark.createDataFrame([(0, "a b c d e spark", 1.0), (
        1, "b d", 0.0), (2, "spark f g h", 1.0), (3, "hadoop mapreduce", 0.0)],
                                     columns + ["label"])

    # Configure an ML pipeline, which consists of three stages: tokenizer, hashingTF, and lr.
    tokenizer = Tokenizer(inputCol="text", outputCol="words")
    hashingTF = HashingTF(
        inputCol=tokenizer.getOutputCol(), outputCol="features")
    lr = LogisticRegression(maxIter=10, regParam=0.001)
    pipeline = Pipeline(stages=[tokenizer, hashingTF, lr])

    # Fit the pipeline to training documents.
    model = pipeline.fit(training)

    # Prepare test documents, which are unlabeled (id, text) tuples.
    test = spark.createDataFrame([(4, "spark i j k"), (5, "l m n"), (
        6, "spark hadoop spark"), (7, "apache hadoop")], columns)

    # Make predictions on test documents and print columns of interest.
    prediction = model.transform(test)
    selected = prediction.select("id", "text", "probability", "prediction")
    for row in selected.collect():
        rid, text, prob, prediction = row
        print("(%d, %s) --> prob=%s, prediction=%f" % (rid, text, str(prob),
                                                       prediction))

    # test predict function
    print(predict(spark, model,
                  [json.dumps((np.random.randint(1000), "spark abcd"))]))

    try:
        clipper = init_clipper()

        try:
            clipper.register_application(app_name, model_name, "strings",
                                         "default_pred", 10000000)
            time.sleep(1)
            response = requests.post(
                "http://localhost:1337/%s/predict" % app_name,
                headers=headers,
                data=json.dumps({
                    'input':
                    json.dumps((np.random.randint(1000), "spark abcd"))
                }))
            result = response.json()
            if response.status_code != requests.codes.ok:
                print("Error: %s" % response.text)
                raise BenchmarkException("Error creating app %s" % app_name)

            version = 1
            clipper.deploy_pyspark_model(model_name, version, predict, model,
                                         spark.sparkContext, "strings")
            time.sleep(10)
            num_preds = 25
            num_defaults = 0
            for i in range(num_preds):
                response = requests.post(
                    "http://localhost:1337/%s/predict" % app_name,
                    headers=headers,
                    data=json.dumps({
                        'input':
                        json.dumps((np.random.randint(1000), "spark abcd"))
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

            version += 1
            clipper.deploy_pyspark_model(model_name, version, predict, model,
                                         spark.sparkContext, "strings")
            time.sleep(10)
            num_preds = 25
            num_defaults = 0
            for i in range(num_preds):
                response = requests.post(
                    "http://localhost:1337/%s/predict" % app_name,
                    headers=headers,
                    data=json.dumps({
                        'input':
                        json.dumps((np.random.randint(1000), "spark abcd"))
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

        except BenchmarkException as e:
            print(e)
            clipper.stop_all()
            spark.stop()
            sys.exit(1)
        else:
            spark.stop()
            clipper.stop_all()
            print("ALL TESTS PASSED")

    except Exception as e:
        print(e)
        clipper = Clipper("localhost")
        clipper.stop_all()
        spark.stop()
        sys.exit(1)
