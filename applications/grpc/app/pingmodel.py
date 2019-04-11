import grpc

import model_pb2
import model_pb2_grpc

import sys

def run(ip, port):
    channel = grpc.insecure_channel('%s:%s'%(ip, port))
    stub = model_pb2_grpc.PredictServiceStub(channel)
    response = stub.Ping(model_pb2.hi(msg = 'Ping test'))
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    ip = "localhost"
    port = "22222"

    if len(sys.argv) > 1:
        ip = sys.argv[1]
        port = sys.argv[2]

    run(ip, port)