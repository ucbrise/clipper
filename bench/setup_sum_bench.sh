#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

DEFAULT_MODEL_NAME="bench_sum"
DEFAULT_MODEL_VERSION=1

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
cd $DIR

. set_bench_env_vars.sh

echo "Starting sum_container"
python ../containers/python/sum_container.py

cd -
