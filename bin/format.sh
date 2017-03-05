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
cd  $DIR/..

num_violations="$(find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
    | xargs clang-format -style=file -output-replacements-xml \
    | grep -c "<replacement ")"

if [ $num_violations -eq 0 ]; then
    exit 0
else
    echo "Found $num_violations Clang-Format violations"
    exit 1
fi
