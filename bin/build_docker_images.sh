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

# Build the Python model containers
cd $DIR/../containers/python
time docker build -t clipper/py-rpc -f RPCDockerfile ./
time docker build -t clipper/sklearn_cifar_container -f SklearnCifarDockerfile ./
time docker build -t clipper/tf_cifar_container -f TensorFlowCifarDockerfile ./
time docker build -t clipper/noop_container -f NoopDockerfile ./
