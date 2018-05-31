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
num_cpp_violations="$(find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
    | xargs clang-format -style=file -output-replacements-xml \
    | grep -c "<replacement ")"

if [ $num_cpp_violations -eq 0 ]; then
    echo "Passed C++ Clang-Format check"
else
    echo "Found $num_cpp_violations C++ style violations."
    echo "Run format_code.sh to fix."
    find ./src -not \( -path ./src/libs -prune \) -name '*pp' -print \
        | xargs clang-format -style=file
    exit 1
fi


# Run clang-format
num_java_violations="$(find . -not \( -path ./src/libs -prune \) -name '*.java' -print \
    | xargs clang-format -style=file -output-replacements-xml \
    | grep -c "<replacement ")"

if [ $num_java_violations -eq 0 ]; then
    echo "Passed Java Clang-Format check"
else
    echo "Found $num_java_violations Java style violations"
    echo "Run format_code.sh to fix."
    find . -name '*.java' -print | xargs clang-format -style=file
    exit 1
fi

# Run Python formatter
export PYTHONPATH=$CLIPPER_ROOT/bin/yapf
num_py_violations="$(find . -name '*.py' -print | egrep -v "yapf|ycm|googletest|dlib" \
    | xargs python $CLIPPER_ROOT/bin/yapf/yapf -d | grep -c "@@")"

if [ $num_py_violations -eq 0 ]; then
    echo "Passed Python PEP8 check"
else
    echo "Found $num_py_violations Python PEP8 format violations"

    echo "Current yapf version"

    git submodule status | grep "yapf" | cat

    echo "Current python version"

    python --version | cat

    find . -name '*.py' -print | egrep -v "yapf|ycm|googletest|dlib" \
        | xargs python $CLIPPER_ROOT/bin/yapf/yapf -d
    exit 1
fi

exit 0
