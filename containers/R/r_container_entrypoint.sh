#!/usr/bin/env bash

set -e
set -u
set -o pipefail

CLIPPER_MODEL_PATH=/home/docker/model.rds
CLIPPER_SAMPLE_INPUT_PATH=/home/docker/sample_input.rds

Rscript serve_model.R -m $CLIPPER_MODEL_PATH -s $CLIPPER_SAMPLE_INPUT_PATH -n $CLIPPER_MODEL_NAME -v $CLIPPER_MODEL_VERSION -i $CLIPPER_IP -p $CLIPPER_PORT
