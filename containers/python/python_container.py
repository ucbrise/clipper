from __future__ import print_function
import rpc
import os
import sys
import importlib
import pickle
import numpy as np
np.set_printoptions(threshold=np.nan)

sys.path.append(os.path.abspath("/lib/"))
sys.path.append(os.path.abspath("/model/modules"))
from clipper_admin import cloudpickle

IMPORT_ERROR_RETURN_CODE = 3


def load_predict_func(file_path):
    with open(file_path, 'r') as serialized_func_file:
        return cloudpickle.load(serialized_func_file)


def load_modules(modules_list_path):
    with open(modules_list_path, 'rb') as f:
        module_fnames = pickle.load(f)
        for m in module_fnames:
            importlib.import_module(m)


class PythonContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)

        modules_folder_path = "{dir}/modules/".format(dir=path)
        sys.path.append(os.path.abspath(modules_folder_path))
        modules_list_path = "{dir}/modules.txt".format(dir=path)
        load_modules(modules_list_path)

        predict_fname = "predict_func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)

    def predict_ints(self, inputs):
        preds = self.predict_func(inputs)
        return [str(p) for p in preds]

    def predict_floats(self, inputs):
        preds = self.predict_func(inputs)
        return [str(p) for p in preds]

    def predict_doubles(self, inputs):
        preds = self.predict_func(inputs)
        return [str(p) for p in preds]

    def predict_bytes(self, inputs):
        preds = self.predict_func(inputs)
        return [str(p) for p in preds]

    def predict_strings(self, inputs):
        preds = self.predict_func(inputs)
        return [str(p) for p in preds]


if __name__ == "__main__":
    print("Starting PythonContainer container")
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

    print("Initializing Python function container")
    sys.stdout.flush()
    sys.stderr.flush()

    try:
        model = PythonContainer(model_path, input_type)
        rpc_service = rpc.RPCService()
        rpc_service.start(model, ip, port, model_name, model_version,
                          input_type)
    except ImportError as e:
        print(e)
        sys.exit(IMPORT_ERROR_RETURN_CODE)
