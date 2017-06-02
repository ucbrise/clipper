from __future__ import print_function
from sklearn.externals import joblib
import rpc
import os
import numpy as np
import scipy as sp
import warnings
warnings.filterwarnings("ignore", category=FutureWarning)
from pandas import *
import pandas.rpy.common as com
from rpy2.robjects.packages import importr
import rpy2.robjects as ro
from rpy2.robjects.packages import importr
stats = importr('stats')
base = importr('base')
import sys
if sys.version_info[0] < 3: 
    from StringIO import StringIO
else:
    from io import StringIO

np.set_printoptions(threshold=np.nan)


class MyRContainer(rpc.ModelContainerBase):
    def __init__(self, path):
        print("initiating MyRContainer")
        self.model = base.readRDS(path)  #ro.r('an_R_model <- readRDS(path)')
        print("Loaded %s model" % type(self.model), file=sys.stderr)
        self.path = path

    def predict_strings(self, inputs):
        #print(inputs)
        TESTDATA=StringIO(inputs[0])
        df = pandas.read_csv(TESTDATA, sep=";")
        rdf=com.convert_to_r_dataframe(df)
        preds=stats.predict(self.model,rdf)
        #print(preds)
        return [str(p) for p in preds]




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

    rds_names=[
           l for l in os.listdir(model_path) if os.path.splitext(l)[-1] == ".rds"
    ] 
    print("these .rds files are Found")
    print(rds_names)

    if len(rds_names) != 1:
        print("Found %d *.rds files. Expected 1" % len(rds_names))
        sys.exit(1)
    rds_path = os.path.join(model_path, rds_names[0])
    print(rds_path, file=sys.stdout)
    
    model=MyRContainer(rds_path)
    rpc_service = rpc.RPCService()
    rpc_service.start(model, ip, port, model_name, model_version, input_type)
