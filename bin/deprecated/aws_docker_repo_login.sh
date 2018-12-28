#!/usr/bin/env bash

set +x

RETVAL=$(aws ecr get-login --no-include-email --region us-west-1)
eval "$RETVAL"

set -x
