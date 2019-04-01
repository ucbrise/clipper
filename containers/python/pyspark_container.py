from __future__ import print_function
import rpc
import os
import sys
import json

import numpy as np
import cloudpickle

import pyspark
from pyspark import SparkConf, SparkContext
from pyspark.sql import SparkSession
import importlib

IMPORT_ERROR_RETURN_CODE = 3


def load_predict_func(file_path):
    if sys.version_info < (3, 0):
        with open(file_path, 'r') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)
    else:
        with open(file_path, 'rb') as serialized_func_file:
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
        if issubclass(ModelClass,
                      pyspark.ml.pipeline.PipelineModel) or issubclass(
                          ModelClass, pyspark.ml.base.Model):
            model = ModelClass.load(model_path)
        else:
            model = ModelClass.load(spark.sparkContext, model_path)
    return model


class PySparkContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)
        modules_folder_path = "{dir}/modules/".format(dir=path)
        sys.path.append(os.path.abspath(modules_folder_path))
        predict_fname = "func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)
        self.spark = SparkSession\
            .builder\
            .appName("clipper-pyspark")\
            .config("spark.ui.enabled", "false")\
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
    rpc_service = rpc.RPCService()
    try:
        model = PySparkContainer(rpc_service.get_model_path(),
                                 rpc_service.get_input_type())
        sys.stdout.flush()
        sys.stderr.flush()
    except ImportError:
        sys.exit(IMPORT_ERROR_RETURN_CODE)
    rpc_service.start(model)
