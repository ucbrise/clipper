#!/usr/bin/env bash

set -x
set -e
set -u
set -o pipefail

# Printout for timeout debug
date

# check color
wget https://gist.githubusercontent.com/hSATAC/1095100/raw/ee5b4d79aee151248bdafa8b8412497a5a688d42/256color.pl
perl 256color.pl
exit 1

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


# Let the user start this script from anywhere in the filesystem.
cd $DIR/..
tag=$(<VERSION.txt)

CLIPPER_REGISTRY='clippertesting'
sha_tag=$(git rev-parse --verify --short=10 HEAD)
KAFKA_ADDRESS="ci.simon-mo.com:32775"

function clean_up {
    # Perform program exit housekeeping
    echo Master CI Process...
    docker kill fluentd-$sha_tag
    echo "Cleanup exit code: $?"
    sleep 2
    exit
}
trap clean_up SIGHUP SIGINT SIGTERM EXIT


# launch fluentd forward
pushd ./bin/shipyard/fluentd
docker build -t $CLIPPER_REGISTRY/fluentd-jenkins .
docker run --rm -d \
    --name fluentd-$sha_tag \
    --network host \
    -e KAFKA_ADDRESS=$KAFKA_ADDRESS \
    $CLIPPER_REGISTRY/fluentd-jenkins
popd


# Build docker images
docker build -t shipyard ./bin/shipyard
docker run --rm shipyard \
  --sha-tag $sha_tag \
  --namespace $CLIPPER_REGISTRY \
  --clipper-root $(pwd) \
  --config clipper_docker.cfg.py \
  --kafka-address $KAFKA_ADDRESS \
  --no-push > Makefile
cat Makefile
make -j 6 kubernetes_test_containers
# curl 
# make -j 10 all

# Build docker images
#./bin/build_docker_images.sh

#echo "Pushing the following images"
#cat ./bin/clipper_docker_images.txt

# Push docker images
#while read in; do docker push "$in"; done < ./bin/clipper_docker_images.txt

#CLIPPER_REGISTRY=$(docker info | grep Username | awk '{ print $2 }')

# Run tests
# docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
#     -e CLIPPER_K8S_CERT_AUTH=$CLIPPER_K8S_CERT_AUTH \
#     -e CLIPPER_K8S_CLIENT_CERT=$CLIPPER_K8S_CLIENT_CERT \
#     -e CLIPPER_K8S_CLIENT_KEY=$CLIPPER_K8S_CLIENT_KEY \
#     -e CLIPPER_K8S_PASSWORD=$CLIPPER_K8S_PASSWORD \
#     -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
#     -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
#     -e CLIPPER_REGISTRY=$CLIPPER_REGISTRY \
#     -e CLIPPER_TESTING_DOCKERHUB_PASSWORD=$CLIPPER_TESTING_DOCKERHUB_PASSWORD \
#     $CLIPPER_REGISTRY/unittests:$sha_tag

# Python 3 unittests
# docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
#     -e CLIPPER_K8S_CERT_AUTH=$CLIPPER_K8S_CERT_AUTH \
#     -e CLIPPER_K8S_CLIENT_CERT=$CLIPPER_K8S_CLIENT_CERT \
#     -e CLIPPER_K8S_CLIENT_KEY=$CLIPPER_K8S_CLIENT_KEY \
#     -e CLIPPER_K8S_PASSWORD=$CLIPPER_K8S_PASSWORD \
#     -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
#     -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
#     -e CLIPPER_REGISTRY=$CLIPPER_REGISTRY \
#     -e CLIPPER_TESTING_DOCKERHUB_PASSWORD=$CLIPPER_TESTING_DOCKERHUB_PASSWORD \
#     $CLIPPER_REGISTRY/py35tests:$sha_tag

docker kill fluentd-$sha_tag