from concurrent import futures
import base64
import time 
import os

import model_pb2
import model_pb2_grpc
import proxy_pb2
import proxy_pb2_grpc

import predict as predict_fn

import grpc


class PredictService(model_pb2_grpc.PredictServiceServicer):
    
    # def GetEncode(self, request, context):
    #     return test_pb2.encodetext(enctransactionID = encoding(request.pttransactionID),
    #                                         encproperties = encoding(request.ptproperties),
    #                                         encsenderID = request.ptsenderID)

    def __init__(self, model_name, model_port, proxy_name, proxy_port):
        self.model_name = model_name
        self.model_port = model_port
        self.proxy_name = proxy_name
        self.proxy_port = proxy_port 
        
    def SetProxy(self, request, context):
        print("Received SetProxy:{request}\n".format(request=request))

        self.proxy_name = request.proxyName
        self.proxy_port = request.proxyPort
        


    def Predict(self, request, context):
        print("received request:{request}\n".format(request=request))
        input_type = request.inputType
        input_stream = request.inputStream

<<<<<<< HEAD

        if (self.proxy_name == None or self.proxy_port == None):
            return model_pb2.response(status = "ProxyNotSet")
=======
        print("Begin predicting")
>>>>>>> 86400a612bd67aed6386f53c366baa82b0b2c0db

        output = predict_fn.predict(input_stream)

#        print("goes here")

#        return test_pb2.response(status = output)



        channel = grpc.insecure_channel('{proxy_name}:{proxy_port}'.format(
            proxy_name = self.proxy_name,
            proxy_port = self.proxy_port
        ))
        stub = proxy_pb2_grpc.ProxyServiceStub(channel)
        response = stub.Return(proxy_pb2.input(
            inputType = "string",
            inputStream = output
        ))
        print('Predicted output [{output}] sent to {proxy}:{response}'.format(
            output = output,
            proxy = self.proxy_name,
            response = response.status
        ))

<<<<<<< HEAD
        return model_pb2.response(status = "Sucessful")

=======
        return test_pb2.response(status = "Sucessful")
        return output
>>>>>>> 86400a612bd67aed6386f53c366baa82b0b2c0db
        

def serve():

    model_name = os.environ["MODEL_NAME"]
    model_port = os.environ["MODEL_PORT"]
    proxy_name = None# os.environ["PROXY_NAME"]
    proxy_port = None# os.environ["PROXY_PORT"]

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=2))
    service = PredictService(model_name, model_port, proxy_name, proxy_port)
    model_pb2_grpc.add_PredictServiceServicer_to_server(service,server)
#    server.add_insecure_port('[::]:22222')

    server.add_insecure_port('[::]:{port}'.format(port=model_port))
    server.start()
    print("Server started")
    try:
        while True:
            time.sleep(60*60*24)
    except KeyboardInterrupt:
        server.stop(0)


if __name__ == '__main__':
    serve()