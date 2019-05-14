#!/usr/bin/env bash
set -x
set -e
set -u
set -o pipefail

# Note
# This script is used to generate makefile.
# Because shipyard.py has package dependencies, we run them in docker container. 

# Let the user start this script from anywhere in the filesystem.
unset CDPATH
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

# We will build all images and push them to 
# dockerhub under $CLIPPER_REGISTRY/{image_name}:sha_tag
CLIPPER_REGISTRY="${CLIPPER_REGISTRY:-clippertesting}"
sha_tag="${CLIPPER_TAG:-$(git rev-parse --verify --short=10 HEAD)}"
push_version_flag="--no-push"

# Jenkins will merge the PR, however we will use the unmerged
# sha to tag our image. 
# https://stackoverflow.com/questions/3601515/how-to-check-if-a-variable-is-set-in-bash
if [ -z ${ghprbActualCommit+x} ]
    then echo "We are not in Jenkins"
    else 
        sha_tag=`echo $ghprbActualCommit | cut -c-10`
fi

# Travis does the same
# If the variable is unset or equal to empty string -> skip
if [ -z ${TRAVIS_PULL_REQUEST_SHA+x} ] || [ -z "$TRAVIS_PULL_REQUEST_SHA"]
    then echo "Not a travis PR Build"
    else 
        sha_tag=`echo $TRAVIS_PULL_REQUEST_SHA | cut -c-10`
fi


# If we are in jenkins and JOB_NAME == Clipper
# Then we are not in PR build. 
# In Clipper Job, we will build and publish to Clipper dockerhub. 
if [ ${JOB_NAME+x} ] && [ $JOB_NAME == "Clipper" ]
    then
        CLIPPER_REGISTRY="clipper"
        push_version_flag="--push"
fi

# Here we use shipyard to generate the Makefile to accelerate
# container building with dependency.
docker build -t shipyard ./bin/shipyard
docker run --rm shipyard \
  --sha-tag $sha_tag \
  --namespace $CLIPPER_REGISTRY \
  --clipper-root $(pwd) \
  --config clipper_docker.cfg.py \
  $push_version_flag > CI_build.Makefile

docker run --rm shipyard \
  --sha-tag $sha_tag \
  --namespace $CLIPPER_REGISTRY \
  --clipper-root $(pwd) \
  --config clipper_test.cfg.py \
  $push_version_flag > CI_test.Makefile