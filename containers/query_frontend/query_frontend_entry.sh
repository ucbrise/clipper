#!/usr/bin/env sh

ARGS=$@

cmd() {
    /clipper/release/src/frontends/query_frontend ${ARGS}
}

ulimit -c unlimited

cmd & pid=$!

wait ${pid}

RESULT=$?

exit ${RESULT}