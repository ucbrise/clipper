#!/usr/bin/env bash

set -e
set -u
set -o pipefail

trap "exit" INT TERM
trap "kill 0" EXIT

unset CDPATH
# One-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
cd $DIR

# If the IP env variable is not defined, attempt to set it
# to the current AWS host's IP.
if [ -z ${IP+x} ]; then
	. export_aws_ip.sh
fi

. set_bench_env_vars.sh $MODEL_NAME $MODEL_VERSION $IP

echo "Starting noop_container"
python ../container/noop_container.py 

cd -
