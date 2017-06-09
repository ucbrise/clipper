from __future__ import print_function
from sklearn.externals import joblib
import rpc
import os
import numpy as np
from pandas import *
from rpy2.robjects import r, pandas2ri
from rpy2.robjects.packages import importr
import rpy2.robjects as ro
stats = importr('stats')
base = importr('base')
import sys
if sys.version_info[0] < 3: 
    from StringIO import StringIO
else:
    from io import StringIO

np.set_printoptions(threshold=np.nan)


class RContainer(rpc.ModelContainerBase):
    def __init__(self, path):
        print("initiating MyRContainer")
        self.model = base.readRDS(path)  
        print("Loaded %s model" % type(self.model), file=sys.stderr)
        self.path = path

    def predict_strings(self, inputs):
        #this method expects dataframe which is specific to model_name and is encoded as string
        #The string is converted to a pandas dataframe and further to an R dataframe before passing to predict function as illustrated below. 
        TESTDATA=StringIO(inputs[0])
        df = pandas.read_csv(TESTDATA, sep=";")
        pandas2ri.activate()
        r_dataframe = pandas2ri.py2ri(df)
        preds=stats.predict(self.model,r_dataframe)
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

    if len(rds_names) != 1:
        print("Found %d *.rds files. Expected 1" % len(rds_names))
        sys.exit(1)
    rds_path = os.path.join(model_path, rds_names[0])
    print(rds_path, file=sys.stdout)
    
    model=RContainer(rds_path)
    rpc_service = rpc.RPCService()
    rpc_service.start(model, ip, port, model_name, model_version, input_type)
