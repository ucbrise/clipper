#!/usr/bin/env bash

set -e
set -u
set -o pipefail

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $DIR/..
./configure
cd debug
make -j2 unittests
if ! type "redis-server" &> /dev/null; then
    echo -e "\nERROR:"
    echo -e "\tUnit tests require Redis. Please install redis-server"
    echo -e "\tand make sure it's on your PATH.\n"
    exit 1
fi

# start the Redis test-server if it's not already running
redis-server --port 34256 &> /dev/null &

./src/libclipper/libclippertests
./src/frontends/frontendtests

# Kills all background jobs.
# Will kill redis if it was started as part of this script.
trap 'kill $(jobs -p) &> /dev/null' EXIT
