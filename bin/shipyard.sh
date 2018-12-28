#!/usr/bin/env bash
set -x
set -e
set -u
set -o pipefail

# Let the user start this script from anywhere in the filesystem.
unset CDPATH
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

# We will build all images and push them to 
# dockerhub under clippertesting/{image_name}:sha_tag
export CLIPPER_REGISTRY='clippertesting'
sha_tag=$(git rev-parse --verify --short=10 HEAD)

# Jenkins will merge the PR, however we will use the unmerged
# sha to tag our image. 
# https://stackoverflow.com/questions/3601515/how-to-check-if-a-variable-is-set-in-bash
if [ -z ${ghprbActualCommit+x}]
    then echo "We are not in Jenkins"
    else sha_tag=`echo $ghprbActualCommit | cut -c-10`
fi

# Here we use shipyard to tool to generate the Makefile to accelerate
# container building with dependency.
docker build -t shipyard ./bin/shipyard
docker run --rm shipyard \
  --sha-tag $sha_tag \
  --namespace $CLIPPER_REGISTRY \
  --clipper-root $(pwd) \
  --config clipper_docker.cfg.py \
  --no-push > Makefile

echo "@@@@@ Makefile @@@@@"
cat Makefile
echo "@@@@@ Makefile @@@@@"

docker run --rm shipyard \
  --sha-tag $sha_tag \
  --namespace $CLIPPER_REGISTRY \
  --clipper-root $(pwd) \
  --config clipper_test.cfg.py \
  --no-push > CI_test.Makefile
