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

if [ $# -ne 0 ] && [ $# -ne 4 ]; then
	echo "Usage: ./build_bench_docker_images.sh [<sum_model_name> <sum_model_version> <noop_model_name> <noop_model_version>]"
	exit 1
fi

# Build the Clipper Docker images
# Assume local clipper/py-rpc base image (if exists) or pulled image is correct

docker build -t clipper/py-rpc:$tag -f ./Py2RPCDockerfile ./
docker build -t clipper/py3-rpc:$tag -f ./Py3RPCDockerfile ./
if [ $# -eq 0 ]; then
	time docker build build --build-arg CODE_VERSION=$tag -t clipper/sum-bench:$tag -f dockerfiles/SumBenchDockerfile ./
	time docker build build --build-arg CODE_VERSION=$tag -t clipper/noop-bench:$tag -f dockerfiles/NoopBenchDockerfile ./
else
	echo $1
	echo $2
	echo $3
	echo $4
	time docker build build --build-arg CODE_VERSION=$tag -t clipper/sum-bench:$tag -f dockerfiles/SumBenchDockerfile ./ --build-arg MODEL_NAME="$1" --build-arg MODEL_VERSION="$2"
	time docker build build --build-arg CODE_VERSION=$tag -t clipper/noop-bench:$tag -f dockerfiles/NoopBenchDockerfile ./ --build-arg MODEL_NAME="$3" --build-arg MODEL_VERSION="$4"
fi

cd -
