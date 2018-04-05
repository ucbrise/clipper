from __future__ import print_function
import sys
sys.path.insert(0, '../../clipper_admin')
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import numpy as np
import signal

import pickle
from collections import deque
from random import shuffle
from time import time, sleep

import clipper_admin.metrics as metrics

with open('vectorizer.pickle', 'rb') as f:
    vectorizer = pickle.load(f)
with open('logistic_regressor.pickle', 'rb') as f:
    lr = pickle.load(f)
with open('email_msg.pickle', 'rb') as f:
    queries = pickle.load(f)


def predict_spam(inp):
    metrics.add_metric('custom_vectorization_time_ms', 'Histogram',
                       'Time it takes to use tfidf transform',
                       [0.1, 0.5, 0.8, 1.0, 1.2])
    metrics.add_metric('custom_lr_time_ms', 'Histogram',
                       'Time it takes to use logistic regression',
                       [0.03, 0.05, 0.06, 0.1])
    metrics.add_metric('custom_choice_probability', 'Histogram',
                       'The logistic regressor probability output',
                       [0.5, 0.7, 0.9, 1.0])
    metrics.add_metric('custom_spam_option_counter', 'Counter',
                       'The number of spam classified')
    metrics.add_metric('custom_ham_option_counter', 'Counter',
                       'The number of ham classfied')
    metrics.add_metric('custom_char_count', 'Histogram',
                       'The number of characters',
                       [10, 50, 100, 300, 500, 800, 1200, 2000])
    metrics.add_metric('custom_word_count', 'Histogram', 'The number of words',
                       [10, 50, 100, 150, 200])

    string = inp[0]
    metrics.report_metric('custom_char_count', len(string))
    metrics.report_metric('custom_word_count', len(string.split()))

    t1 = time()
    vect = vectorizer.transform(inp).toarray()
    t2 = time()
    result = lr.predict(vect)
    t3 = time()
    prob = lr.predict_proba(vect)[0, result]

    metrics.report_metric('custom_vectorization_time_ms', (t2 - t1) * 1000)
    metrics.report_metric('custom_lr_time_ms', (t3 - t2) * 1000)
    metrics.report_metric('custom_choice_probability', prob)
    if int(result) == 1:
        metrics.report_metric('custom_spam_option_counter', 1.0)
    else:
        metrics.report_metric('custom_ham_option_counter', 1.0)

    return list(result)


def predict(addr, x, batch=False):
    url = "http://%s/ham-spam-classifier/predict" % addr

    req_json = json.dumps({'input': x})

    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


# Stop Clipper on Ctrl-C
def signal_handler(signal, frame):
    print("Stopping Clipper...")
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.stop_all()
    sys.exit(0)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal_handler)
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.start_clipper()
    python_deployer.create_endpoint(clipper_conn, "ham-spam-classifier",
                                    "strings", predict_spam)
    sleep(2)

    try:
        queue = deque(queries)
        while True:
            query = queue.pop()
            print("Length {} query sent!".format(len(query)))
            predict(clipper_conn.get_query_addr(), query)
            queue.append(query)
            shuffle(queue)
            sleep(0.2)
    except Exception as e:
        print(e)
        clipper_conn.stop_all()
