#!/usr/bin/env bash

set -e
set -u
set -o pipefail

function clean_up {
    # Perform program exit housekeeping
    # echo Background jobs: $(jobs -l)
    # echo
    # echo Killing jobs
    echo Exiting...
    kill $(jobs -p) &> /dev/null
    echo
    sleep 2
    # echo Remaining background jobs: $(jobs -l)
    exit
}

function randomize_redis_port {
    set +e  # turn of exit on command fail
    REDIS_PORT=$((34256 + RANDOM % 1000))
    lsof -i :$REDIS_PORT &> /dev/null

    if [ $? -eq 0 ]; then # existing port in use found
      while true; do
        REDIS_PORT=$(($REDIS_PORT + RANDOM % 1000))
        lsof -i :$REDIS_PORT &> /dev/null
        if [ $? -eq 1 ]; then  # port not in use
          break
        fi
      done
    fi
    echo "$REDIS_PORT"
}

trap clean_up SIGHUP SIGINT SIGTERM

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
cd $DIR/..
./configure
cd debug
# make all to make sure all the binaries compile
make -j2 all unittests
if ! type "redis-server" &> /dev/null; then
    echo -e "\nERROR:"
    echo -e "\tUnit tests require Redis. Please install redis-server"
    echo -e "\tand make sure it's on your PATH.\n"
    exit 1
fi

randomize_redis_port

# start Redis on the test port if it's not already running
redis-server --port $REDIS_PORT &> /dev/null &

./src/libclipper/libclippertests --redis_port $REDIS_PORT
./src/frontends/frontendtests --redis_port $REDIS_PORT
./src/management/managementtests --redis_port $REDIS_PORT

clean_up
