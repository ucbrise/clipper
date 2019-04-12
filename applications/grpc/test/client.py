import grpc

import ..app.model_pb2
import ..app.model_pb2_grpc

def run(ip, port):
    channel = grpc.insecure_channel('%s:%s')
    stub = model_pb2_grpc.PredictServiceStub(channel)
    response = stub.Predict(model_pb2.input(inputType = 'string', inputStream = 'This is a plain text transaction'))
    print('Response {res}'.format(res=response.status))

if __name__ == "__main__":
    ip = "localhost"
    port = "22222"
    run()