from __future__ import print_function
import rpc
import os
import sys
import json

import numpy as np

import cloudpickle

import onnx
import caffe2.python.onnx.backend

import importlib

IMPORT_ERROR_RETURN_CODE = 3

MODEL_RELATIVE_PATH = "model.onnx"


def load_predict_func(file_path):
    if sys.version_info < (3, 0):
        with open(file_path, 'r') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)
    else:
        with open(file_path, 'rb') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)


def load_onnx_into_caffe2_model(model_path):
    model = onnx.load(model_path)
    prepared_backend = caffe2.python.onnx.backend.prepare(model, device="CPU")
    return prepared_backend


class Caffe2Container(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)
        modules_folder_path = "{dir}/modules/".format(dir=path)
        sys.path.append(os.path.abspath(modules_folder_path))
        predict_fname = "func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)

        onnx_path = os.path.join(path, MODEL_RELATIVE_PATH)

        self.model = load_onnx_into_caffe2_model(onnx_path)

    def predict_ints(self, inputs):
        preds = self.predict_func(self.model, inputs)
        return [str(p) for p in preds]

    def predict_floats(self, inputs):
        preds = self.predict_func(self.model, inputs)
        return [str(p) for p in preds]

    def predict_doubles(self, inputs):
        preds = self.predict_func(self.model, inputs)
        return [str(p) for p in preds]

    def predict_bytes(self, inputs):
        preds = self.predict_func(self.model, inputs)
        return [str(p) for p in preds]

    def predict_strings(self, inputs):
        preds = self.predict_func(self.model, inputs)
        return [str(p) for p in preds]


if __name__ == "__main__":
    print("Starting Caffe2Container container")
    rpc_service = rpc.RPCService()
    try:
        model = Caffe2Container(rpc_service.get_model_path(),
                                rpc_service.get_input_type())
        sys.stdout.flush()
        sys.stderr.flush()
    except ImportError:
        sys.exit(IMPORT_ERROR_RETURN_CODE)
    rpc_service.start(model)
