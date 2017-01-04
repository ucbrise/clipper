from __future__ import print_function
# import sys
import json
# import os
import requests
# import random
from datetime import datetime
# import time


def add_app():
    url = "http://localhost:1338/admin/add_app"
    req_json = json.dumps({
     "name": "my_app",
     "candidate_models": [{"model_name": "m", "model_version": 1}],
     "input_type": "integers",
     "output_type": "double",
     "selection_policy": "simple_policy",
     "latency_slo_micros": 10000
    })

    headers = {'Content-type': 'application/json'}
    # x_str = ", ".join(["%d" % a for a in x])
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


if __name__ == '__main__':
    add_app()
