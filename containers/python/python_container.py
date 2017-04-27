from __future__ import print_function
import rpc
import os
import sys

import numpy as np
np.set_printoptions(threshold=np.nan)

sys.path.append(os.path.abspath("/lib/"))
import pywrencloudpickle


def load_predict_func(file_path):
    with open(file_path, 'r') as serialized_func_file:
        return pywrencloudpickle.load(serialized_func_file)


class PythonContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        print("Initializing Python function container")
        self.input_type = rpc.string_to_input_type(input_type)
        predict_fname = "predict_func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)
        print("Loaded prediction function")

    def predict_ints(self, input_item):
        if self.input_type != rpc.INPUT_TYPE_INTS:
            self._log_incorrect_input_type(rpc.INPUT_TYPE_INTS)
            return
        return str(self.predict_func(input_item))

    def predict_floats(self, input_item):
        if self.input_type != rpc.INPUT_TYPE_FLOATS:
            self._log_incorrect_input_type(rpc.INPUT_TYPE_FLOATS)
            return
        return str(self.predict_func(input_item))

    def predict_doubles(self, input_item):
        if self.input_type != rpc.INPUT_TYPE_DOUBLES:
            self._log_incorrect_input_type(rpc.INPUT_TYPE_DOUBLES)
            return
        return str(self.predict_func(input_item))

    def predict_bytes(self, input_item):
        if self.input_type != rpc.INPUT_TYPE_BYTES:
            self._log_incorrect_input_type(rpc.INPUT_TYPE_BYTES)
            return
        return str(self.predict_func(input_item))

    def predict_string(self, input_item):
        if self.input_type != rpc.INPUT_TYPE_STRINGS:
            self._log_incorrect_input_type(rpc.INPUT_TYPE_STRINGS)
            return
        return str(self.predict_func(input_item))

    def _log_incorrect_input_type(self, input_type):
        incorrect_input_type = rpc.input_type_to_string(input_type)
        correct_input_type = rpc.input_type_to_string(self.input_type)
        print(
            "Attempted to use prediction function for input type {incorrect_input_type}.\
            This model-container was configured accept data for input type {correct_input_type}"
            .format(
                incorrect_input_type=incorrect_input_type,
                correct_input_type=correct_input_type))


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

    model = PythonContainer(model_path, input_type)
    rpc_service = rpc.RPCService()
    rpc_service.start(model, ip, port, model_name, model_version, input_type)
