#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

DEFAULT_MODEL_NAME="bench_sklearn_cifar"
DEFAULT_MODEL_VERSION=1

if [ $# -ne 1 ] && [ $# -ne 3 ]; then
	echo "Usage: ./bench/setup_sklearn_bench.sh <path_to_cifar_python_dataset> [[<model_name> <model_version> [<clipper_ip>]]]"
	exit 1
fi

cifar_path=$1

python bench/bench_init.py $cifar_path

shift

. bench/set_bench_env_vars.sh

echo "Starting sklearn_cifar_container"
python containers/python/sklearn_cifar_container.py 
