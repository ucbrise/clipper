from concurrent import futures
import base64
import time 
import os

import model_pb2
import model_pb2_grpc

import proxy_pb2
import proxy_pb2_grpc


import grpc


def get_pre(dag, model_id):
    pre_list = []
    lines = dag.split('\n')
    node_num = int(lines[0])
    edge_num = int(lines[1])

    nodes = lines[2:2+node_num]

    edges = lines[2+node_num:2+node_num+edge_num]

    for edge in edges:
        
        first = edge.split(',')[0]
        second = edge.split(',')[1]

        if second == model_id:
            prenode = nodes[int(first)-1]
            pre_proxy = prenode.split(',')[8]
            pre_list.append(pre_proxy)

    return pre_list 


def get_post(dag, model_id):
    post_list = []
    lines = dag.split('\n')
    node_num = int(lines[0])
    edge_num = int(lines[1])

    nodes = lines[2:2+node_num]

    edges = lines[2+node_num:2+node_num+edge_num]

    for edge in edges:
        
        first = edge.split(',')[0]
        second = edge.split(',')[1]

        if first == model_id:
            postnode = nodes[int(second)-1]
            post_proxy = postnode.split(',')[8]
            post_list.append(post_proxy)

    return post_list 


class ProxyService(proxy_pb2_grpc.ProxyServiceServicer):
    
    # def GetEncode(self, request, context):
    #     return test_pb2.encodetext(enctransactionID = encoding(request.pttransactionID),
    #                                         encproperties = encoding(request.ptproperties),
    #                                         encsenderID = request.ptsenderID)

    def __init__(self, proxy_name, proxy_port):
        self.model_name = None
        self.model_port = None
        self.model_id = None
        self.proxy_name = proxy_name
        self.proxy_port = proxy_port
        self.pre_list = []
        self.post_list = []
        self.dag = None
        
    def SetModel(self, request, context):
        print("Received SetModel:{request}\n".format(request=request))

        self.model_name = request.modelName
        self.model_port = request.modelPort
        self.model_id = request.modelId

        return proxy_pb2.response(status = "SetModel Sucessful")


    def SetDAG(self, request, context):

        self.dag = request.dag_

        self.pre_list = get_pre(self.dag, self.model_id)
        self.post_list = get_post(self.dag, self.model_id)
        
        return proxy_pb2.response(status = "SetDAG Sucessful for model %s(%s): \n pre_list:%s \n post_list:%s"%(self.model_name, self.model_id, self.pre_list,self.post_list))


    def Predict(self, request, context):

        print("received request:{request}\n".format(request=request))
#        input_type = request.inputType
#        input_stream = request.inputStream

#        output = predict_fn.predict(input_stream)

#        print("goes here")

#        return test_pb2.response(status = output)

        if (self.model_name == None or self.model_port == None):
            return model_pb2.response(status = "ModelNotSet")

        channel = grpc.insecure_channel('{model_name}:{model_port}'.format(
            model_name = self.model_name,
            model_port = self.model_port
        ))
        stub = model_pb2_grpc.PredictServiceStub(channel)
        response = stub.Predict(model_pb2.input(
            inputType = request.inputType,
            inputStream = request.inputStream
        ))
        print('Prediction request [{request}] sent to {model}:{response}'.format(
            request = request,
            model = self.model_name,
            response = response.status
        ))

        input_type = request.inputType
        input_stream = response.status

        reply = "============Output From Model%s ============\n%s\n"%(self.model_name, response.status)

        for proxy in self.post_list:
            channel = grpc.insecure_channel('{proxy_name}:{proxy_port}'.format(
                proxy_name = proxy,
                proxy_port = self.proxy_port
            ))
            stub = proxy_pb2_grpc.ProxyServiceStub(channel)
            response = stub.Predict(proxy_pb2.input(
                inputType = input_type,
                inputStream = input_stream
            ))
            print('Prediction request [{request}] sent to {proxy}:{response}'.format(
                request = request,
                proxy = proxy,
                response = response.status
            ))

            reply = reply + response.status

        return proxy_pb2.response(status = reply)
        
    def Return(self, request, context):

 



        return proxy_pb2.response(status = "Sucessful")

    def Ping(self, request, context):

        print("received request:{request}\n".format(request=request))
        hi_msg = request.msg

        if (self.model_name == None or self.model_port == None):
            return model_pb2.response(status = "ModelNotSet")

        '''
        Connect to model 
        '''
        channel = grpc.insecure_channel('{model_name}:{model_port}'.format(
            model_name = self.model_name,
            model_port = self.model_port
        ))

        stub = model_pb2_grpc.PredictServiceStub(channel)
        response = stub.Ping(model_pb2.hi(
            msg = "This is %s \n"%(self.proxy_name)
        ))

        r = response.status

        print("finished ping model")

        for proxy in self.post_list:
            print("started ping proxy %s"%(proxy))
            channel = grpc.insecure_channel('{proxy_name}:{proxy_port}'.format(
                proxy_name = proxy,
                proxy_port = self.proxy_port
            ))
            stub = proxy_pb2_grpc.ProxyServiceStub(channel)
            response = stub.Ping(proxy_pb2.hi(
                msg = " This is %s \n"%(self.proxy_name)
            ))
            
            r = r+response.status 

        print("return ping request")
        r = "This is %s \n"%(self.proxy_name) + r
        return proxy_pb2.response(status = r)





        

def serve():

    proxy_name = os.environ["PROXY_NAME"]
    proxy_port = os.environ["PROXY_PORT"]

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=2))
    service = ProxyService(proxy_name, proxy_port)
    proxy_pb2_grpc.add_ProxyServiceServicer_to_server(service,server)
#    server.add_insecure_port('[::]:22222')

    server.add_insecure_port('[::]:{port}'.format(port=proxy_port))
    server.start()
    print("Proxy Server Started --- %s listening on port %s"%(proxy_name, proxy_port))
    try:
        while True:
            time.sleep(60*60*24)
    except KeyboardInterrupt:
        server.stop(0)


if __name__ == '__main__':
    serve()