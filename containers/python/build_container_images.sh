#!/usr/bin/env sh

# first build base image
docker build -t clipper/py-rpc -f RPCDockerfile ./
time docker build -t clipper/sklearn_cifar_container -f SklearnCifarDockerfile ./
docker push clipper/sklearn_cifar_container
time docker build -t clipper/noop_container -f NoopDockerfile ./

