#!/bin/bash

python3 -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. model.proto
python3 -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. proxy.proto
