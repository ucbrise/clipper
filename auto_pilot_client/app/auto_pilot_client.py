
#!/usr/bin/env python
import argparse
import grpc
from google.protobuf.timestamp_pb2 import Timestamp

import model_pb2_grpc
import model_pb2
import proxy_pb2_grpc
import proxy_pb2
import prediction_pb2_grpc
import prediction_pb2

def setModel(proxy_ip, proxy_port, container_name, container_count, container_ip, container_port):
    channel_proxy = grpc.insecure_channel('{proxy_ip}:{proxy_port}'.format(
        proxy_ip=proxy_ip,
        proxy_port="22223"
    ))
    stub_proxy = prediction_pb2_grpc.ProxyServerStub(channel_proxy)
    response = stub_proxy.SetModel(prediction_pb2.modelinfo(
        modelName=container_name,
        modelId=int(container_count),
        modelPort=22222,
        modelIp=container_ip
    ))
    print('SetModel call OK with response{res}'.format(res=response.status))

def setProxy(container_ip, container_port, proxy_name, proxy_port):
    #tells the model container its proxy's info
    channel_container = grpc.insecure_channel('{container_ip}:{container_port}'.format(
        container_ip=container_ip,
        container_port="22222"
    ))
    stub_container = model_pb2_grpc.PredictServiceStub(
        channel_container)
    response = stub_container.SetProxy(model_pb2.proxyinfo(
        proxyName=proxy_name,
        proxyPort="22223"
    ))

    print('SetProxy call OK with response{res}'.format(res=response.status))


def setDAG(proxy_ip, proxy_port, expanded_dag):
    channel_proxy = grpc.insecure_channel('{proxy_ip}:{proxy_port}'.format(
        proxy_ip=proxy_ip,
        proxy_port="22223"
    ))
    stub_proxy = prediction_pb2_grpc.ProxyServerStub(channel_proxy)
    response = stub_proxy.SetDAG(prediction_pb2.dag(dag_=expanded_dag))

    print('SetDAG call OK with response{res}'.format(res=response.status))

def autoPilotPredict(ip, port):
    timestamp = Timestamp()
    timestamp.GetCurrentTime()
    channel = grpc.insecure_channel('%s:%s'%(ip, port))
    stub = prediction_pb2_grpc.ProxyServerStub(channel)
    response = stub.downstream(prediction_pb2.request(input_ = model_pb2.input(inputType = 'string', inputStream = '1'),src_uri = "localhost", seq = 1, req_id =1, timestamp = timestamp))
    print('Response\n{res}'.format(res=response.status))


def main():

    parser = argparse.ArgumentParser(description='Grpc client')

    parser.add_argument('--setmodel', nargs=6, type=str)
    parser.add_argument('--setproxy', nargs=4, type=str)
    parser.add_argument('--setdag', nargs="+", type=str)
    parser.add_argument('--autopilot', nargs=2, type=str)
    
                       
    args = parser.parse_args()

    if args.autopilot is not None:
        #print(args.stock)
        autoPilotPredict(args.stock[0], args.stock[1])

    if args.setmodel is not None:
        #print(args.setmodel)
        setModel(args.setmodel[0],args.setmodel[1],args.setmodel[2],args.setmodel[3], args.setmodel[4], args.setmodel[5])

    if args.setproxy is not None:
        #print(args.setproxy)
        setProxy(args.setproxy[0],args.setproxy[1],args.setproxy[2],args.setproxy[3])

    if args.setdag is not None:
        #print(args.setdag) 
        expanded_dag = ""
        for line in args.setdag[2:]:
            expanded_dag = expanded_dag + line + "\n"
        setDAG(args.setdag[0],args.setdag[1],expanded_dag)

        

if __name__ == '__main__':
    main()



