import requests
import sys
import numpy as np
import json
import scipy as sp
import warnings
warnings.filterwarnings("ignore", category=FutureWarning)
from pandas import *
import pandas.rpy.common as com
from rpy2.robjects.packages import importr
import rpy2.robjects as ro


stats = importr('stats')
base = importr('base')

def start_prediction(df,App,host):
    #converting dataframes to strings so that they can be sent via requests.post() method in call_prediction
    columns=len(df.columns.values)
    rows=len(df)
    head=""
    
    for i in range(0,columns):
        head=head+df.columns.values[i]+";"
    head=head[:-1]
    head=head+"\n"

    for query in range(0,rows):
        tail=""
        for i in range(0,columns):
            tail=tail+str(df.values[query][i])+";"
        tail=tail[:-1]
        tail=tail+"\n"

        query_string=head+tail
        #print(query_string)
        call_prediction(query_string,query,App,host)
        
        


def call_prediction(query_string,query,App,host):
    url = "http://%s:1337/%s/predict" % (host,App)
    req_json = json.dumps({'uid': 0, 'input':query_string })
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("response : %i" %(query))
    print("'%s', latency is : %f ms" % (r.text, latency))
    print("\n")

