from __future__ import print_function
import rpc
import os
import numpy as np
import pandas as pd
from rpy2.robjects import r, pandas2ri
from rpy2.robjects.packages import importr
import rpy2.robjects as ro
import sys
if sys.version_info < (3, 0):
    from StringIO import StringIO
else:
    from io import StringIO
stats = importr('stats')
base = importr('base')


class RContainer(rpc.ModelContainerBase):
    def __init__(self, path):
        self.model = base.readRDS(path)
        print("Loaded %s model" % type(self.model), file=sys.stderr)
        self.path = path

    def predict_strings(self, inputs):
        outputs = []
        for input_csv in inputs:
            csv_handle = StringIO(input_csv)
            pdf = pd.read_csv(csv_handle, sep=";", index_col=0)
            pandas2ri.activate()
            rdf = pandas2ri.py2ri(pdf)
            preds = stats.predict(self.model, rdf)
            make_list = ro.r('as.list')
            make_df = ro.r('data.frame')
            rdf_preds = make_df(make_list(preds))
            pdf_preds = pandas2ri.ri2py(rdf_preds)
            response_csv = pdf_preds.to_csv(sep=";")
            outputs.append(response_csv)
        return outputs


if __name__ == "__main__":
    print("Starting R container")
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
        print("Connecting to Clipper with default port: 7000")

    input_type = "strings"
    model_path = os.environ["CLIPPER_MODEL_PATH"]

    rds_names = [
        l for l in os.listdir(model_path) if os.path.splitext(l)[-1] == ".rds"
    ]

    if len(rds_names) != 1:
        print("Found %d *.rds files. Expected 1" % len(rds_names))
        sys.exit(1)
    rds_path = os.path.join(model_path, rds_names[0])
    print(rds_path, file=sys.stdout)

    model = RContainer(rds_path)
    rpc_service = rpc.RPCService()
    rpc_service.start(model, ip, port, model_name, model_version, input_type)
