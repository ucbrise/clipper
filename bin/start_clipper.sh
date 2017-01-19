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

trap clean_up SIGHUP SIGINT SIGTERM


unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
cd $DIR/..
./configure --release
cd release
make -j2 query_frontend management_frontend
if ! type "redis-server" &> /dev/null; then
    echo -e "\nERROR:"
    echo -e "\tClipper require Redis to run. Please install redis-server"
    echo -e "\tand make sure it's on your PATH.\n"
    exit 1
fi

# start Redis if it's not already running
redis-server &> /dev/null &
sleep 5

# start the query processor frontend
./src/management/management_frontend &
# MANAGER_PID=$!
# echo $MANAGER_PID
# echo $(jobs -p)

# start the query processor frontend
./src/frontends/query_frontend

clean_up
