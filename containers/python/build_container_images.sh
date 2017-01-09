#!/usr/bin/env sh



# first build base image
docker build -t clipper/py-rpc -f RPCDockerfile ./
time docker build -t clipper/sklearn_cifar_container -f SklearnCifarDockerfile ./
time docker build -t clipper/tf_cifar_container -f TensorFlowCifarDockerfile ./
time docker build -t clipper/noop_container -f NoopDockerfile ./

