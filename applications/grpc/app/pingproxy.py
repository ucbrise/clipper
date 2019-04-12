import grpc

import proxy_pb2
import proxy_pb2_grpc

import sys

def run(ip, port):
    channel = grpc.insecure_channel('%s:%s'%(ip, port))
    stub = proxy_pb2_grpc.ProxyServiceStub(channel)
    response = stub.Ping(proxy_pb2.hi(msg = 'Ping test'))
    print('Response\n{res}'.format(res=response.status))

if __name__ == "__main__":
    ip = "localhost"
    port = "22222"

    if len(sys.argv) > 1:
        ip = sys.argv[1]
        port = sys.argv[2]

    run(ip, port)