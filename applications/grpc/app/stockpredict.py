import grpc

import prediction_pb2
import prediction_pb2_grpc
import model_pb2
from google.protobuf.timestamp_pb2 import Timestamp


import sys

def run(ip, port):
    timestamp = Timestamp()
    timestamp.GetCurrentTime()
    channel = grpc.insecure_channel('%s:%s'%(ip, port))
    stub = prediction_pb2_grpc.ProxyServerStub(channel)
    stock_name = "AAPL"
    response = stub.downstream(prediction_pb2.request(input_ = model_pb2.input(inputType = 'string', inputStream = stock_name),src_uri = "localhost", seq = 1, req_id =1, timestamp = timestamp))
    print('Response\n{res}'.format(res=response.status))

if __name__ == "__main__":
    ip = "localhost"
    port = "22222"

    if len(sys.argv) > 1:
        ip = sys.argv[1]
        port = sys.argv[2]

    run(ip, port)