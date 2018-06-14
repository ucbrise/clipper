#!/usr/bin/env bash

set -e
set -u
set -o pipefail

# Printout for timeout debug
date

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


# Let the user start this script from anywhere in the filesystem.
cd $DIR/..
tag=$(<VERSION.txt)

# Build docker images
./bin/build_docker_images.sh

CLIPPER_REGISTRY=$(docker info | grep Username | awk '{ print $2 }')

# Run tests
docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
    -e CLIPPER_K8S_CERT_AUTH=$CLIPPER_K8S_CERT_AUTH \
    -e CLIPPER_K8S_CLIENT_CERT=$CLIPPER_K8S_CLIENT_CERT \
    -e CLIPPER_K8S_CLIENT_KEY=$CLIPPER_K8S_CLIENT_KEY \
    -e CLIPPER_K8S_PASSWORD=$CLIPPER_K8S_PASSWORD \
    -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
    -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
    -e CLIPPER_REGISTRY=$CLIPPER_REGISTRY \
    $CLIPPER_REGISTRY/unittests:$tag

# Python 3 unittests
docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
    -e CLIPPER_K8S_CERT_AUTH=$CLIPPER_K8S_CERT_AUTH \
    -e CLIPPER_K8S_CLIENT_CERT=$CLIPPER_K8S_CLIENT_CERT \
    -e CLIPPER_K8S_CLIENT_KEY=$CLIPPER_K8S_CLIENT_KEY \
    -e CLIPPER_K8S_PASSWORD=$CLIPPER_K8S_PASSWORD \
    -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
    -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
    -e CLIPPER_REGISTRY=$CLIPPER_REGISTRY \
    $CLIPPER_REGISTRY/py35tests:$tag
