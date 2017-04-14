#!/usr/bin/env sh

# first build base image
docker build -t clipper/py-rpc -f RPCDockerfile ./
time docker build -t clipper/noop-container -f NoopDockerfile ./
time docker build -t clipper/python-container -f PythonContainerDockerfile ./
