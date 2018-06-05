from __future__ import print_function
import rpc
import os
import sys
import json

import numpy as np
import cloudpickle
import torch
import importlib
from torch import nn
from torch.autograd import Variable
import torch.nn.functional as F

IMPORT_ERROR_RETURN_CODE = 3

PYTORCH_WEIGHTS_RELATIVE_PATH = "pytorch_weights.pkl"
PYTORCH_MODEL_RELATIVE_PATH = "pytorch_model.pkl"


def load_predict_func(file_path):
    if sys.version_info < (3, 0):
        with open(file_path, 'r') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)
    else:
        with open(file_path, 'rb') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)


def load_pytorch_model(model_path, weights_path):
    if sys.version_info < (3, 0):
        with open(model_path, 'r') as serialized_model_file:
            model = cloudpickle.load(serialized_model_file)
    else:
        with open(model_path, 'rb') as serialized_model_file:
            model = cloudpickle.load(serialized_model_file)

    model.load_state_dict(torch.load(weights_path))
    return model


class PyTorchContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)
        modules_folder_path = "{dir}/modules/".format(dir=path)
        sys.path.append(os.path.abspath(modules_folder_path))
        predict_fname = "func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)

        torch_model_path = os.path.join(path, PYTORCH_MODEL_RELATIVE_PATH)
        torch_weights_path = os.path.join(path, PYTORCH_WEIGHTS_RELATIVE_PATH)
        self.model = load_pytorch_model(torch_model_path, torch_weights_path)

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
    print("Starting PyTorchContainer container")
    rpc_service = rpc.RPCService()
    try:
        model = PyTorchContainer(rpc_service.get_model_path(),
                                 rpc_service.get_input_type())
        sys.stdout.flush()
        sys.stderr.flush()
    except ImportError:
        sys.exit(IMPORT_ERROR_RETURN_CODE)
    rpc_service.start(model)
