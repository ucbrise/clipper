#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

export CLIPPER_MODEL_NAME="bench_noop"
export CLIPPER_MODEL_VERSION="1"
export CLIPPER_MODEL_PATH="model/"

python containers/python/noop_container.py 
