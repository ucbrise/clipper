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

# Build the query frontend
time docker build -t clipper/query_frontend -f QueryFrontendDockerfile ./
time docker build -t clipper/management_frontend -f ManagementFrontendDockerfile ./
