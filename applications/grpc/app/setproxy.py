import grpc

import model_pb2
import model_pb2_grpc

import sys

def run(ip, port, proxy_name, proxy_port):
    channel = grpc.insecure_channel('%s:%s'%(ip, port))
    stub = model_pb2_grpc.PredictServiceStub(channel)
    response = stub.SetProxy(model_pb2.proxyinfo(proxyName=proxy_name, proxyPort=proxy_port))
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    ip = "localhost"
    port = "22222"
    proxy_name = ""
    proxy_port = ""

    if len(sys.argv) > 1:
        ip = sys.argv[1]
        port = sys.argv[2]
        proxy_name = sys.argv[3]
        proxy_port = sys.argv[4]

    run(ip, port, proxy_name, proxy_port)