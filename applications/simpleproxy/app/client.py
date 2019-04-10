import grpc

import proxy_pb2
import proxy_pb2_grpc

def run():
    channel = grpc.insecure_channel('172.18.0.7:22223')
    stub = proxy_pb2_grpc.ProxyServiceStub(channel)
   # response = stub.Predict(proxy_pb2.input(inputType = 'string', inputStream = 'This is a plain text transaction'))

    response = stub.SetModel(proxy_pb2.modelinfo(modelName = 'test', modelId = '1', modelPort='22222'))
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    run()