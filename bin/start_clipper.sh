#!/usr/bin/env bash

set -e
set -u
set -o pipefail

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $DIR/..
./configure --release
cd release
make -j2 query_frontend
if ! type "redis-server" &> /dev/null; then
    echo -e "\nERROR:"
    echo -e "\tClipper require Redis to run. Please install redis-server"
    echo -e "\tand make sure it's on your PATH.\n"
    exit 1
fi

# start Redis if it's not already running
redis-server &> /dev/null &

./src/frontends/query_frontend

# Kills all background jobs.
# Will kill redis if it was started as part of this script.
trap 'kill $(jobs -p) &> /dev/null' SIGINT SIGTERM EXIT
