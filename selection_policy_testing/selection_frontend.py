import threading
from collections import deque
import socket
import json
import redis
import datetime

def select(state, query):
    if query['lol']:
        return [1, 2]
    else:
        return [3]

def combine(state, results):
    state += 1
    return (state, results[1])

class SelectionPolicy (threading.Thread):
    def __init__(self, qu, re, uid_cache, id_cache, model_id_que):
        super(SelectionPolicy, self).__init__()
        self.name = 'Selection Policy Thread'
        self.queries = qu
        self.redis_inst = re
        self.uid_cache = uid_cache
        self.id_cache = id_cache
        self.model_id_que = model_id_que

    def run(self):
        while True:
            if len(self.queries) > 0:
                query = self.queries.popleft()
                (timestamp, state) = eval(self.redis_inst.get(query['user_id']))
                self.uid_cache[(query['user_id'], timestamp)] = (state, 1)
                self.id_cache[query['query_id']] = (query['user_id'], timestamp)
                self.model_id_que.append((query, select(state, query)))

class TaskExecutor (threading.Thread):
    def __init__(self, mids, pred):
        super(TaskExecutor, self).__init__()
        self.model_id_que = mids
        self.predictions = pred

    def run(self):
        while True:
            if len(self.model_id_que) > 0:
                task = self.model_id_que.popleft()
                preds = [i * 2 for i in task[1]]
                self.predictions.append((task[0]['query_id'], preds))

class Combiner (threading.Thread):
    def __init__(self, pred, outgoing):
        super(Combiner, self).__init__()
        self.preds = pred
        self.outgoing = outgoing

    def run(self):
        while True:
            if len(self.preds) > 0:
                query = self.preds.popleft()
                # state =

class Cache:
    def __init__(self, refcounts=False):
        self.cache = {}
        self.rCount = refcounts

    def __getitem__(self, item):
        if item in self.cache:
            return self.cache[item]

    def __setitem__(self, key, value):
        if key not in self.cache or not self.rCount:
            self.cache[key] = value
        else:
            self.cache[key][1] += 1

    def pop(self, key):
        if key not in self.cache:
            return True
        state = self.cache[key]
        if self.rCount:
            self.cache[key][1] -= 1
            # To keep the dictionary as small as possible.
            if self.cache[key][1] == 0:
                del self.cache[key]
        return state

    def popstate(self, key):
        return self.pop(key)[0]

if __name__ == '__main__':
    re = redis.Redis()
    re.set('rdurrani', (datetime.datetime.now(), b'state'))
    queries = deque()
    q_predictions = deque()
    model_ids = deque()
    qf_sock = socket.socket()
    qf_sock.connect((socket.gethostname(), 8080))
    uid_cache = Cache(True)
    id_cache = Cache()
    sp = SelectionPolicy(queries, re, uid_cache, id_cache)
    te = TaskExecutor(model_ids, q_predictions)

    sp.start()
    while True:
        query = json.loads(qf_sock.recv(1024).decode('utf-8'))
        queries.append(query)