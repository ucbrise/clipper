#!/usr/bin/env bash

set -e
set -u

# TODO: Remove the /tmp volume mount in the Clipper admin refactor PR
docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp clipper/unittests:develop
