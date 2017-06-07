#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

cifar_path=$1

python bench/bench_init.py $cifar_path

export CLIPPER_MODEL_NAME="bench_sklearn_cifar"
export CLIPPER_MODEL_VERSION="1"
export CLIPPER_MODEL_PATH="model/"

python containers/python/sklearn_cifar_container.py 
