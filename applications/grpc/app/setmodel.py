import grpc

import proxy_pb2
import proxy_pb2_grpc

import sys

def run(ip, port, model_name, model_port):
    channel = grpc.insecure_channel('%s:%s'%(ip, port))
    stub = proxy_pb2_grpc.ProxyServiceStub(channel)
    response = stub.SetModel(proxy_pb2.modelinfo(modelName=proxy_name, modelPort=proxy_port, modelId="1"))
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    ip = "localhost"
    port = "22222"
    model_name = ""
    model_port = ""

    if len(sys.argv) > 1:
        ip = sys.argv[1]
        port = sys.argv[2]
        proxy_name = sys.argv[3]
        proxy_port = sys.argv[4]

    run(ip, port,  model_name, model_port)