import socket
import json
import time

if __name__ == '__main__':
    sel_sock = socket.socket()
    sel_sock.bind((socket.gethostname(), 8080))
    sel_sock.listen(1)
    sel_addr, a = sel_sock.accept()
    count = 0
    while True:
        query = json.dumps({'user_id': 'rdurrani', 'query_id':count, 'query':[1, 2, 3, 4]})
        count += 1
        sel_addr.send(query.encode('utf-8'))
        time.sleep(0.05)