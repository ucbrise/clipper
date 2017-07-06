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

# Build the Clipper Docker images
# Assume local (if exists) clipper/py-rpc base image or pulled image is correct
time docker build -t clipper/same-prediction-bench -f SamePredictionBenchDockerfile ./
time docker build  -t clipper/noop-bench -f NoopBenchDockerfile ./
cd -
