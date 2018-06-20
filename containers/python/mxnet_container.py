from __future__ import print_function
import rpc
import os
import sys
import json

import numpy as np
import cloudpickle
import mxnet as mx
import importlib

IMPORT_ERROR_RETURN_CODE = 3

MXNET_MODEL_RELATIVE_PATH = "mxnet_model"


def load_predict_func(file_path):
    if sys.version_info < (3, 0):
        with open(file_path, 'r') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)
    else:
        with open(file_path, 'rb') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)


# load mxnet model from serialized dir
def load_mxnet_model(model_path):
    mxnet_model_path = os.path.join(model_path, MXNET_MODEL_RELATIVE_PATH)

    data_shapes = json.load(
        open(os.path.join(model_path, "mxnet_model_metadata.json")))

    sym, arg_params, aux_params = mx.model.load_checkpoint(
        prefix=mxnet_model_path, epoch=0)

    mxnet_model = mx.mod.Module(symbol=sym)
    mxnet_model.bind(data_shapes["data_shapes"])
    mxnet_model.set_params(arg_params, aux_params)

    return mxnet_model


class MXNetContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)
        modules_folder_path = "{dir}/modules/".format(dir=path)
        sys.path.append(os.path.abspath(modules_folder_path))
        predict_fname = "func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)

        # load mxnet model from serialized dir
        self.model = load_mxnet_model(path)

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
    print("Starting MXNetContainer container")
    rpc_service = rpc.RPCService()
    try:
        model = MXNetContainer(rpc_service.get_model_path(),
                               rpc_service.get_input_type())
        sys.stdout.flush()
        sys.stderr.flush()
    except ImportError:
        sys.exit(IMPORT_ERROR_RETURN_CODE)
    rpc_service.start(model)
