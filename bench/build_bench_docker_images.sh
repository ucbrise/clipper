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

if [ $# -ne 0 ] && [ $# -ne 4 ]; then
	echo "Usage: ./build_bench_docker_images.sh [<sum_model_name> <sum_model_version> <noop_model_name> <noop_model_version>]"
	exit 1
fi

# Build the Clipper Docker images
# Assume local clipper/py-rpc base image (if exists) or pulled image is correct
if [ $# -eq 0 ]; then
	time docker build -t clipper/sum-bench -f SumBenchDockerfile ./
	time docker build  -t clipper/noop-bench -f NoopBenchDockerfile ./
else
	echo $1
	echo $2
	echo $3
	echo $4
	time docker build -t clipper/sum-bench -f SumBenchDockerfile ./ --build-arg MODEL_NAME="$1" --build-arg MODEL_VERSION="$2"
	time docker build  -t clipper/noop-bench -f NoopBenchDockerfile ./ --build-arg MODEL_NAME="$3" --build-arg MODEL_VERSION="$4"
fi

cd -
