#!/usr/bin/env bash

set -e
set -u
set -o pipefail

pushd ..
find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
    | xargs clang-format -style=file -i
popd
