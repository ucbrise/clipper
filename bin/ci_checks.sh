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
tag=$(<VERSION.txt)
cd -

# Test docker login
docker pull 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:$tag

# Let the user start this script from anywhere in the filesystem.
$DIR/check_format.sh
$DIR/run_unittests.sh
