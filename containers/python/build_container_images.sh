#!/usr/bin/env sh

# first build base image
docker build -t clipper/py-rpc -f dockerfiles/RPCDockerfile ./
time docker build -t clipper/noop-container -f dockerfiles/NoopDockerfile ./
time docker build -t clipper/python-container -f dockerfiles/PythonContainerDockerfile ./
