from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

import findspark
findspark.init()
from pyspark.ml import Pipeline
from pyspark.ml.classification import LogisticRegression
from pyspark.ml.feature import HashingTF, Tokenizer
from pyspark.sql import SparkSession

from test_utils import (create_container_manager, BenchmarkException, headers,
                        log_clipper_state, SERVICE)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin_v2" % cur_dir))
import clipper_admin as cl
from clipper_admin.deployers.pyspark import deploy_pyspark_model

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "pyspark_pipeline_test"
model_name = "pyspark_pipeline"

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


def run_test():
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
        cm = create_container_manager(
            SERVICE, cleanup=True, start_clipper=True)

        try:
            cl.register_application(cm, app_name, "strings", "default_pred",
                                    10000000)
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
            deploy_pyspark_model(cm, model_name, version, "strings", predict,
                                 model, spark.sparkContext)
            cl.link_model_to_app(cm, app_name, model_name)
            time.sleep(30)
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
                if response.status_code == requests.codes.ok and result["default"]:
                    num_defaults += 1
            if num_defaults > 0:
                print("Error: %d/%d predictions were default" % (num_defaults,
                                                                 num_preds))
            if num_defaults > num_preds / 2:
                raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                         (app_name, model_name, version))

            version += 1
            deploy_pyspark_model(cm, model_name, version, "strings", predict,
                                 model, spark.sparkContext)
            time.sleep(30)
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
                if response.status_code == requests.codes.ok and result["default"]:
                    num_defaults += 1
            if num_defaults > 0:
                print("Error: %d/%d predictions were default" % (num_defaults,
                                                                 num_preds))
            if num_defaults > num_preds / 2:
                raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                         (app_name, model_name, version))
        except BenchmarkException as e:
            log_clipper_state(cm)
            logger.exception("BenchmarkException")
            cm = create_container_manager(
                SERVICE, cleanup=True, start_clipper=False)
            sys.exit(1)
        else:
            spark.stop()
            cm = create_container_manager(
                SERVICE, cleanup=True, start_clipper=False)
            logger.info("ALL TESTS PASSED")
    except Exception as e:
        logger.exception("Exception")
        cm = create_container_manager(
            SERVICE, cleanup=True, start_clipper=False)
        sys.exit(1)


if __name__ == "__main__":
    run_test()
