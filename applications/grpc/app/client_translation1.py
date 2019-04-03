import grpc
import test_pb2
import test_pb2_grpc
import numpy as np
from scipy.io import wavfile

def run():
    channel = grpc.insecure_channel('localhost:22222')
    stub = test_pb2_grpc.PredictServiceStub(channel)
    
    fs, data = wavfile.read('test.wav')

    string = str(list(data))

    response = stub.Predict(test_pb2.input(inputType = 'string', inputStream = string))
    
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    run()