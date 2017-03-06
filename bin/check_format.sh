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


CLIPPER_ROOT=$DIR/..
echo $CLIPPER_ROOT
cd  $CLIPPER_ROOT

set +e

# Run clang-format
num_violations="$(find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
    | xargs clang-format -style=file -output-replacements-xml \
    | grep -c "<replacement ")"

if [ $num_violations -eq 0 ]; then
    echo "Passed Clang-Format check"
else
    echo "Found $num_violations Clang-Format violations"
    find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
        | xargs clang-format -style=file -output-replacements-xml
    exit 1
fi

# Run Python formatter
export PYTHONPATH=$CLIPPER_ROOT/bin/yapf
num_py_violations="$(find . -name '*.py' -print | egrep -v "yapf|ycm|googletest" \
    | xargs python $CLIPPER_ROOT/bin/yapf/yapf -d | grep -c "@@")"

if [ $num_py_violations -eq 0 ]; then
    echo "Passed Python PEP8 check"
else
    echo "Found $num_py_violations Python PEP8 format violations"
    exit 1
fi

exit 0
