from __future__ import print_function
from sklearn.externals import joblib
import rpc
import os
import sys
import numpy as np
np.set_printoptions(threshold=np.nan)


class SklearnCifarContainer(rpc.ModelContainerBase):
    def __init__(self, path):
        self.model = joblib.load(path)
        print("Loaded %s model" % type(self.model), file=sys.stderr)
        self.path = path

    def predict_doubles(self, inputs):
        preds = self.model.predict(inputs).astype(np.float32)
        return preds

    # def predict_ints(self, inputs):
    #     mean, sigma = np.mean(inputs, axis=1), np.std(inputs, axis=1)
    #     np.place(sigma, sigma == 0, 1.)
    #     normalized_inputs = np.transpose((inputs.T - mean) / sigma)
    #     preds = self.model.predict(normalized_inputs).astype(np.float32)
    #     return preds



if __name__ == "__main__":
    print("Starting Sklearn Cifar container")
    try:
        model_name = os.environ["CLIPPER_MODEL_NAME"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_NAME environment variable must be set",
              file=sys.stdout)
        sys.exit(1)
    try:
        model_version = os.environ["CLIPPER_MODEL_VERSION"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_VERSION environment variable must be set",
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
        print("Connecting to Clipper with default port: 7000")

    input_type = "doubles"
    model_path = os.environ["CLIPPER_MODEL_PATH"]
    pkl_names = [l for l in os.listdir(model_path) if
                 os.path.splitext(l)[-1] == ".pkl"]
    print(pkl_names)
    if len(pkl_names) != 1:
        print("Found %d *.pkl files. Expected 1" % len(pkl_names))
        sys.exit(1)
    pkl_path = os.path.join(model_path, pkl_names[0])
    print(pkl_path, file=sys.stdout)
    model = SklearnCifarContainer(pkl_path)
    rpc.start(model, ip, port, model_name, model_version, input_type)
