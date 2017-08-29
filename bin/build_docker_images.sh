#!/usr/bin/env bash

set -e
set -u
set -o pipefail

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


# Let the user start this script from anywhere in the filesystem.
cd $DIR/..

tag=$(<VERSION.txt)

# Build the Clipper Docker images
time docker build -t clipper/lib_base:$tag -f ./ClipperLibBaseDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/query_frontend:$tag -f QueryFrontendDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/management_frontend:$tag -f ManagementFrontendDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/unittests:$tag -f ClipperTestsDockerfile ./

# Tag and push the latest version of the Clipper Docker images to the container registry
# for the Kubernetes testing cluster
docker tag clipper/query_frontend:$tag 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:$tag
docker push 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:$tag
docker tag clipper/management_frontend:$tag 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/management_frontend:$tag
docker push 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/management_frontend:$tag
cd -

# Build Spark JVM Container
cd $DIR/../containers/jvm
time docker build -t clipper/spark-scala-container:$tag -f SparkScalaContainerDockerfile ./
cd -

# Build the Python model containers
cd $DIR/..

# first build base image
time docker build -t clipper/py-rpc:$tag -f ./RPCDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/sum-container:$tag -f ./SumDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/noop-container:$tag -f ./NoopDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/python-closure-container:$tag -f ./PyClosureContainerDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/pyspark-container:$tag -f ./PySparkContainerDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/sklearn_cifar_container:$tag -f ./SklearnCifarDockerfile ./
time docker build --build-arg CODE_VERSION=$tag -t clipper/tf_cifar_container:$tag -f ./TensorFlowCifarDockerfile ./
# TODO: uncomment
# time docker build --build-arg CODE_VERSION=$tag -t clipper/r_python_container:$tag -f ./RPythonDockerfile ./
cd -
