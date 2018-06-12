import threading
from collections import deque
import socket
import json
import redis
import datetime

def select(state, query):
    if query['select_flag']:
        return [1, 2]
    else:
        return [3]

def combine(state, results):
    return max(results)

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
            self.cache[key] = (self.cache[key][0], self.cache[key][1] + 1)

    def pop(self, key):
        if key not in self.cache:
            return True
        state = self.cache[key]
        if self.rCount:
            self.cache[key] = (self.cache[key][0], self.cache[key][1] - 1)
            # To keep the dictionary as small as possible.
            if self.cache[key][1] == 0:
                del self.cache[key]
        else:
            del self.cache[key]
        return state

    def popstate(self, key):
        return self.pop(key)[0]

class Reciever (threading.Thread):
    def __init__(self, select_q, combine_q, sockt):
        super(Reciever, self).__init__()
        self.sq = select_q
        self.cq = combine_q
        self.sock = sockt
        self.sock.connect((socket.gethostname(), 8080))

    def run(self):
        while True:
            a = self.sock.recv(1).decode('utf-8')
            q = '' + str(a)
            while a != '}':
                a = self.sock.recv(1).decode('utf-8')
                q = q + str(a)
            query = json.loads(q)
            if query['msg'] == 'select':
                self.sq.append(query)
                print('append query', query['query_id'])
            elif query['msg'] == 'combine':
                self.cq.append(query)
                print('append cquery', query['query_id'])

class Sender (threading.Thread):
    def __init__(self, send_que, sock):
        super(Sender, self).__init__()
        self.sq = send_que
        self.sock = sock
        self.sock.connect((socket.gethostname(), 8083))

    def run(self):
        while True:
            if len(self.sq) > 0:
                query = json.dumps(self.sq.popleft())
                self.sock.send(query.encode('utf-8'))

class SelectionPolicy(threading.Thread):
    def __init__(self, query_queue, send_queue, redis_inst, query_cache, id_cache):
        super(SelectionPolicy, self).__init__()
        self.query_queue = query_queue
        self.redis_inst = redis_inst
        self.send_queue = send_queue
        self.query_cache = query_cache
        self.id_cache = id_cache

    def run(self):
        while True:
            if len(self.query_queue) > 0:
                query = self.query_queue.popleft()
                (timestamp, state) = eval(self.redis_inst.get(query['user_id']))
                self.query_cache[(query['user_id'], timestamp)] = (state, 1)
                self.id_cache[query['query_id']] = (query['user_id'], timestamp)
                self.send_queue.append({'id': query['query_id'], 'msg': 'exec', 'mids': select(state, query)})
                print('append send sel', query['query_id'])

class Combiner (threading.Thread):
    def __init__(self, query_queue, send_queue, query_cache, id_cache):
        super(Combiner, self).__init__()
        self.query_queue = query_queue
        self.send_queue = send_queue
        self.query_cache = query_cache
        self.id_cache = id_cache

    def run(self):
        while True:
            if len(self.query_queue) > 0:
                query = self.query_queue.popleft()
                state = self.query_cache[self.id_cache[query['query_id']]][0]
                final_pred = combine(state, query['preds'])
                self.send_queue.append({'id': query['query_id'], 'msg': 'return', 'final_pred':final_pred})
                self.query_cache.pop(self.id_cache[query['query_id']])
                self.id_cache.pop(query['query_id'])
                print('append send combine', query['query_id'])

if __name__ == '__main__':
    re = redis.Redis()
    re.set('rdurrani', (datetime.datetime.now(), b'state'))
    select_queue = deque()
    combine_queue = deque()
    send_queue = deque()
    recieve_sock = socket.socket()
    send_sock = socket.socket()
    query_cache = Cache(refcounts=True)
    id_cache = Cache()
    reciever = Reciever(select_queue, combine_queue, recieve_sock)
    sender = Sender(send_queue, send_sock)
    sel_pol = SelectionPolicy(select_queue, send_queue, re, query_cache, id_cache)
    combiner = Combiner(combine_queue, send_queue, query_cache, id_cache)
    reciever.start()
    sel_pol.start()
    combiner.start()
    sender.start()