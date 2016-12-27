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
make unittests
./src/libclipper/libclippertests
./src/frontends/frontendtests
