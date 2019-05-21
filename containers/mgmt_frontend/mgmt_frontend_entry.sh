#!/usr/bin/env sh

ARGS=$@

cmd() {
    /clipper/release/src/management/management_frontend ${ARGS}
}

ulimit -c unlimited

cmd & pid=$!

wait ${pid}

RESULT=$?

exit ${RESULT}