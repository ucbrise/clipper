#!/usr/bin/env bash

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

# Run clang-format on cpp/hpp files
find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
    | xargs clang-format -style=file -i

# Run clang-format on Java files
find ./src -not \( -path ./src/libs -prune \) -name '*.java' -print \
    | xargs clang-format -style=file -i

# Run Python formatter
export PYTHONPATH=$CLIPPER_ROOT/bin/yapf
find . -name '*.py' -print | egrep -v "yapf|ycm|googletest|dlib" \
    | xargs python $CLIPPER_ROOT/bin/yapf/yapf -i

exit 0
