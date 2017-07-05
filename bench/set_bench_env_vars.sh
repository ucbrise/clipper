#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

DEFAULT_CLIPPER_MODEL_PATH="model/"
DEFAULT_CLIPPER_IP="localhost"

if [ $# -ne 0 ] && [ $# -ne 2 ] && [ $# -ne 3 ]; then
	echo "Usage: ./set_bench_env_vars.sh [[<model_name> <model_version> [<clipper_ip>]]]"
	exit 1
fi

if [ -z ${1+x} ]; then
	echo "No model_name supplied. Setting CLIPPER_MODEL_NAME to $DEFAULT_MODEL_NAME"
	export CLIPPER_MODEL_NAME=$DEFAULT_MODEL_NAME
	echo "No model version supplied. Setting CLIPPER_MODEL_VERSION to $DEFAULT_MODEL_VERSION"
	export CLIPPER_MODEL_VERSION=$DEFAULT_MODEL_VERSION
else
	echo "Setting CLIPPER_MODEL_NAME=$1"
	export CLIPPER_MODEL_NAME=$0
	echo "Setting CLIPPER_MODEL_VERSION=$2"
	export CLIPPER_MODEL_VERSION=$1
fi

if [ -z ${3+x} ]; then
	echo "No clipper_ip supplied. Setting CLIPPER_IP to $DEFAULT_CLIPPER_IP"
	export CLIPPER_IP=$DEFAULT_CLIPPER_IP
else
	echo "Setting CLIPPER_IP to $3"
	export CLIPPER_IP=$3
fi

export CLIPPER_MODEL_PATH=$DEFAULT_CLIPPER_MODEL_PATH
