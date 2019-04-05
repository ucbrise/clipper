import grpc

import test_pb2
import test_pb2_grpc

def run():
    channel = grpc.insecure_channel('localhost:22222')
    stub = test_pb2_grpc.PredictServiceStub(channel)
    response = stub.Predict(test_pb2.input(inputType = 'string', inputStream = 'This is a plain text transaction'))
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    run()