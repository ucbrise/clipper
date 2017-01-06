from __future__ import print_function
import rpc
import os
import sys
import numpy as np


class NoopContainer(rpc.ModelContainerBase):

    def __init__(self):
        pass

    def predict_ints(self, inputs):
        return np.array([np.sum(x) for x in inputs], dtype='float32')

    def predict_floats(self, inputs):
        return np.array([np.sum(x) for x in inputs], dtype='float32')

    def predict_doubles(self, inputs):
        return np.array([np.sum(x) for x in inputs], dtype='float32')

    def predict_bytes(self, inputs):
        return np.array([len(x) for x in inputs], dtype='float32')

    def predict_strings(self, inputs):
        return np.array([len(x) for x in inputs], dtype='float32')


if __name__ == "__main__":
    try:
        model_name = os.environ["CLIPPER_MODEL_NAME"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_NAME environment variable must be set", file=sys.stderr)
        sys.exit(1)
    try:
        model_version = os.environ["CLIPPER_MODEL_VERSION"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_VERSION environment variable must be set", file=sys.stderr)
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
        print("Connecting to Clipper with default port: 7000")

    input_type = "doubles"
    if "CLIPPER_INPUT_TYPE" in os.environ:
        input_type = os.environ["CLIPPER_INPUT_TYPE"]
    else:
        print("Using default input type: doubles")
    model = NoopContainer()
    rpc.start(model, ip, port, model_name, model_version, input_type)
