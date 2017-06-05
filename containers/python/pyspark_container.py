from __future__ import print_function
import rpc
import os
import sys
import json

import numpy as np
np.set_printoptions(threshold=np.nan)

sys.path.append(os.path.abspath("/lib/"))
from clipper_admin import cloudpickle

import findspark
findspark.init()
import pyspark
from pyspark import SparkConf, SparkContext
from pyspark.sql import SparkSession
import importlib

IMPORT_ERROR_RETURN_CODE = 3


def load_predict_func(file_path):
    with open(file_path, 'r') as serialized_func_file:
        return cloudpickle.load(serialized_func_file)


def load_pyspark_model(metadata_path, spark, model_path):
    with open(metadata_path, "r") as metadata:
        metadata = json.load(metadata)
        if "model_class" not in metadata:
            print("Malformed metadata file.")
            sys.exit(1)
        model_class = metadata["model_class"]

        print("Loading %s model from %s" % (model_class, model_path))
        splits = model_class.split(".")
        module = ".".join(splits[:-1])
        class_name = splits[-1]
        ModelClass = getattr(importlib.import_module(module), class_name)
        if model_class == "pyspark.ml.pipeline.PipelineModel":
            model = ModelClass.load(model_path)
        else:
            model = ModelClass.load(spark.sparkContext, model_path)
    return model


class PySparkContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)
        predict_fname = "predict_func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)
        self.spark = SparkSession\
            .builder\
            .appName("clipper-pyspark")\
            .getOrCreate()
        metadata_path = os.path.join(path, "metadata.json")
        spark_model_path = os.path.join(path, "pyspark_model_data")
        self.model = load_pyspark_model(metadata_path, self.spark,
                                        spark_model_path)

    def predict_ints(self, inputs):
        preds = self.predict_func(self.spark, self.model, inputs)
        return [str(p) for p in preds]

    def predict_floats(self, inputs):
        preds = self.predict_func(self.spark, self.model, inputs)
        return [str(p) for p in preds]

    def predict_doubles(self, inputs):
        preds = self.predict_func(self.spark, self.model, inputs)
        return [str(p) for p in preds]

    def predict_bytes(self, inputs):
        preds = self.predict_func(self.spark, self.model, inputs)
        return [str(p) for p in preds]

    def predict_strings(self, inputs):
        preds = self.predict_func(self.spark, self.model, inputs)
        return [str(p) for p in preds]


if __name__ == "__main__":
    print("Starting PySparkContainer container")
    try:
        model_name = os.environ["CLIPPER_MODEL_NAME"]
    except KeyError:
        print(
            "ERROR: CLIPPER_MODEL_NAME environment variable must be set",
            file=sys.stdout)
        sys.exit(1)
    try:
        model_version = os.environ["CLIPPER_MODEL_VERSION"]
    except KeyError:
        print(
            "ERROR: CLIPPER_MODEL_VERSION environment variable must be set",
            file=sys.stdout)
        sys.exit(1)

    ip = "127.0.0.1"
    if "CLIPPER_IP" in os.environ:
        ip = os.environ["CLIPPER_IP"]
    else:
        print("Connecting to Clipper on localhost")

    port = 7000
    if "CLIPPER_PORT" in os.environ:
        port = int(os.environ["CLIPPER_PORT"])
    else:
        print("Connecting to Clipper with default port: {port}".format(
            port=port))

    input_type = "doubles"
    if "CLIPPER_INPUT_TYPE" in os.environ:
        input_type = os.environ["CLIPPER_INPUT_TYPE"]
    else:
        print("Using default input type: doubles")

    model_path = os.environ["CLIPPER_MODEL_PATH"]
    print(model_path)

    print("Initializing PySpark function container")
    sys.stdout.flush()
    sys.stderr.flush()

    try:
        model = PySparkContainer(model_path, input_type)
        rpc_service = rpc.RPCService()
        rpc_service.start(model, ip, port, model_name, model_version,
                          input_type)
    except ImportError:
        sys.exit(IMPORT_ERROR_RETURN_CODE)
