#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

export CLIPPER_MODEL_NAME="bench_sameprediction"
export CLIPPER_MODEL_VERSION="1"
export CLIPPER_MODEL_PATH="model/"
// Set this with curl http://169.254.169.254/latest/meta-data/local-ipv4 from the AWS instance
export CLIPPER_IP="172.31.23.83"

python /containers/python/same_prediction_container.py 
