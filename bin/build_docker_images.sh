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

# Build the Clipper Docker images
time docker build -t clipper/query_frontend -f QueryFrontendDockerfile ./
time docker build -t clipper/management_frontend -f ManagementFrontendDockerfile ./
cd -

# Build Spark JVM Container
cd $DIR/../containers/jvm
time docker build -t clipper/spark-scala-container -f SparkScalaContainerDockerfile ./
cd -

# Build the Python model containers
cd $DIR/..

# first build base image
docker build -t clipper/py-rpc -f ./RPCDockerfile ./
time docker build -t clipper/noop-container -f ./NoopDockerfile ./
time docker build -t clipper/python-container -f ./PythonContainerDockerfile ./
time docker build -t clipper/pyspark-container -f ./PySparkContainerDockerfile ./
time docker build -t clipper/sklearn_cifar_container -f ./SklearnCifarDockerfile ./
time docker build -t clipper/tf_cifar_container -f ./TensorFlowCifarDockerfile ./
cd -
