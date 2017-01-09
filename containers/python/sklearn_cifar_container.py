from __future__ import print_function
import rpc
import os
import sys
import numpy as np
from sklearn.externals import joblib

class SklearnCifarContainer(rpc.ModelContainerBase):
    def __init__(self, path):
        self.model = joblib.load(path)
        print("Loaded %s model" % type(self.model), file=sys.stderr)
        self.path = path

    def predict_ints(self, inputs):
        mean, sigma = np.mean(inputs, axis=1), np.std(inputs, axis=1)
        np.place(sigma, sigma == 0, 1.)
        normalized_inputs = np.transpose((inputs.T - mean) / sigma)
        preds = self.model.predict(normalized_inputs)
        # Change -1 to 0 for binary classification
        preds[np.where(preds == -1)] = 0.
        return preds


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

    input_type = "ints"
    model_path = os.environ["CLIPPER_MODEL_PATH"]
    pkl_names = [l for l in os.listdir(model_path) if os.path.splitext(l)[1] == ".pkl"]
    assert len(pkl_names) == 1
    pkl_path = os.path.join(model_path, pkl_names[0])
    print(pkl_path, file=sys.stderr)
    model = SklearnCifarContainer(pkl_path)
    rpc.start(model, ip, port, model_name, model_version, input_type)
