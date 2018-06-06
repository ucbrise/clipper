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

# Tag and push the latest version of the Clipper Docker images to the container registry
# for the Kubernetes testing cluster
docker tag clipper/query_frontend:$tag 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:$tag
docker push 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:$tag
docker tag clipper/management_frontend:$tag 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/management_frontend:$tag
docker push 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/management_frontend:$tag

# Run tests
docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
    -e CLIPPER_K8S_CERT_AUTH=$CLIPPER_K8S_CERT_AUTH \
    -e CLIPPER_K8S_CLIENT_CERT=$CLIPPER_K8S_CLIENT_CERT \
    -e CLIPPER_K8S_CLIENT_KEY=$CLIPPER_K8S_CLIENT_KEY \
    -e CLIPPER_K8S_PASSWORD=$CLIPPER_K8S_PASSWORD \
    -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
    -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
    clipper/unittests:$tag

# Python 3 unittests
docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
    -e CLIPPER_K8S_CERT_AUTH=$CLIPPER_K8S_CERT_AUTH \
    -e CLIPPER_K8S_CLIENT_CERT=$CLIPPER_K8S_CLIENT_CERT \
    -e CLIPPER_K8S_CLIENT_KEY=$CLIPPER_K8S_CLIENT_KEY \
    -e CLIPPER_K8S_PASSWORD=$CLIPPER_K8S_PASSWORD \
    -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
    -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
    clipper/py35tests:$tag
