#!/bin/bash

cp model_pb2_grpc.py ../applications/simpleproxy/app/
cp model_pb2.py ../applications/simpleproxy/app/
cp proxy_pb2_grpc.py ../applications/simpleproxy/app/
cp proxy_pb2.py ../applications/simpleproxy/app/



cp model_pb2_grpc.py ../applications/grpc/app/
cp model_pb2.py ../applications/grpc/app/
cp proxy_pb2_grpc.py ../applications/grpc/app/
cp proxy_pb2.py ../applications/grpc/app/
cp prediction_pb2_grpc.py ../applications/grpc/app/
cp prediction_pb2.py ../applications/grpc/app/


cp model_pb2_grpc.py ../clipper_admin/clipper_admin/rpc/
cp model_pb2.py ../clipper_admin/clipper_admin/rpc/
cp proxy_pb2_grpc.py ../clipper_admin/clipper_admin/rpc/
cp proxy_pb2.py ../clipper_admin/clipper_admin/rpc/
cp prediction_pb2_grpc.py ../clipper_admin/clipper_admin/rpc/
cp prediction_pb2.py ../clipper_admin/clipper_admin/rpc/

cp model_pb2_grpc.py ../grpcclient/app/
cp model_pb2.py ../grpcclient/app/
cp proxy_pb2_grpc.py ../grpcclient/app/
cp proxy_pb2.py ../grpcclient/app/
cp prediction_pb2_grpc.py ../grpcclient/app/
cp prediction_pb2.py ../grpcclient/app/



rm *.py