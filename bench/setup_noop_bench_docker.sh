#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

export CLIPPER_MODEL_NAME="bench_noop"
export CLIPPER_MODEL_VERSION="1"
export CLIPPER_MODEL_PATH="model/"

# Sets CLIPPER_IP to AWS instance's IP
export CLIPPER_IP=$(curl http://169.254.169.254/latest/meta-data/local-ipv4)

python /containers/python/noop_container.py 
