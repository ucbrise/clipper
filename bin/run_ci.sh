#!/usr/bin/env bash

set -e
set -u

docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock clipper/unittests:develop
