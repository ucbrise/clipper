#!/usr/bin/env bash

NUMCORES=2
#assumes running under CLIPPER_HOME
./configure --cleanup-quiet
git submodule update --init --recursive

./configure

pushd  debug
make -j${NUMCORES}

#start redis-server in a separate window
make unittests

popd
