#!/usr/bin/env bash

set -e
set -u

docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp clipper/unittests:develop
