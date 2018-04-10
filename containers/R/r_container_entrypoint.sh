#!/usr/bin/env bash

set -e
set -u
set -o pipefail

touch /model_is_ready.check
Rscript serve_model.R -m $CLIPPER_MODEL_PATH -n  $CLIPPER_MODEL_NAME -v $CLIPPER_MODEL_VERSION -i $CLIPPER_IP -p $CLIPPER_PORT
