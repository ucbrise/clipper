import socket
import json
import time
import threading
from collections import deque

class Sender(threading.Thread):
    def __init__(self, que, sock):
        super(Sender, self).__init__()
        self.q = que
        sock.listen(1)
        self.sock, a = sock.accept()

    def run(self):
        while True:
            if len(self.q) > 0:
                query = json.dumps(self.q.popleft())
                self.sock.send(query.encode('utf-8'))

def query_generator(count):
    a = count % 2 == 0
    return {'user_id': 'rdurrani', 'query_id': count, 'query': [1, 2, 3, 4], 'msg': 'select',
                                'select_flag':a}

class QueryGen(threading.Thread):
    def __init__(self, que, qs):
        super(QueryGen, self).__init__()
        self.q = que
        self.qs = qs
        self.count = 0

    def run(self):
        while True:
            query = query_generator(self.count)
            self.qs[self.count] = json.dumps(query)
            self.count += 1
            self.q.append(query)
            time.sleep(0.05)

class Reciever(threading.Thread):
    def __init__(self, final_q, exec_q, sock):
        super(Reciever, self).__init__()
        self.fq = final_q
        self.eq = exec_q
        sock.listen(1)
        self.sock, a = sock.accept()

    def run(self):
        while True:
            a = self.sock.recv(1).decode('utf-8')
            q = '' + str(a)
            while a != '}':
                a = self.sock.recv(1).decode('utf-8')
                q = q + str(a)
            query = json.loads(q)
            if query['msg'] == 'exec':
                self.eq.append(query)
            elif query['msg'] == 'return':
                self.fq.append(query)

class TaskExecutor(threading.Thread):
    def __init__(self, task_queue, send_queue, q_dic):
        super(TaskExecutor, self).__init__()
        self.tq = task_queue
        self.sq = send_queue
        self.qd = q_dic

    def run(self):
        while True:
            if len(self.tq) > 0:
                mids = self.tq.popleft()
                query = json.loads(self.qd[mids['id']])
                query['preds'] = [i * 2 for i in mids['mids']]
                query['msg'] = 'combine'
                self.sq.append(query)
                self.qd[mids['id']] = query

class Client(threading.Thread):
    def __init__(self, que, queries):
        super(Client, self).__init__()
        self.q = que
        self.qd = queries

    def run(self):
        while True:
            if len(self.q) > 0:
                q = self.q.popleft()
                query = self.qd[q['id']]
                id = query['query_id']
                correct = (id % 2 == 0 and q['final_pred'] == 4) or (id % 2 == 1 and q['final_pred'] == 6)
                if correct:
                    print('Query', id, 'outputted correctly!')
                else:
                    print('Query', id, 'FAILED!')
                del self.qd[id]

if __name__ == '__main__':
    sel_sock = socket.socket()
    sel_sock.bind((socket.gethostname(), 8080))
    send_s = socket.socket()
    send_s.bind((socket.gethostname(), 8083))
    qs = dict()
    queries = deque()
    exec_queue = deque()
    final_queue = deque()
    qg = QueryGen(queries, qs)
    s = Sender(queries, sel_sock)
    r = Reciever(final_queue, exec_queue, send_s)
    t = TaskExecutor(exec_queue, queries, qs)
    c = Client(final_queue, qs)
    qg.start()
    s.start()
    r.start()
    t.start()
    c.start()