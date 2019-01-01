#!/usr/bin/env bash
set -x
set -e
set -u
set -o pipefail

unset CDPATH

# Note:
# This script will try to run the entire CI process
# including building all the images, pushing the image
# to dockerhub and then run the integration tests. 
# This is script is intended to be ran on Jenkins. 
# However, you can adapt this script to run the entire
# suite in other environment. 

# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
cd $DIR/..
tag=$(<VERSION.txt)

# Be nice to Jenkins
clean_up_jenkins() {
    bash ./bin/cleanup_jenkins.sh
}

# Change the VERSION.txt to current sha_tag
sha_tag=$(git rev-parse --verify --short=10 HEAD)
# Jenkins will merge the PR, however we will use the unmerged
# sha to tag our image. 
# https://stackoverflow.com/questions/3601515/how-to-check-if-a-variable-is-set-in-bash
if [ -z ${ghprbActualCommit+x} ]
    then echo "We are not in Jenkins"
    else 
        sha_tag=`echo $ghprbActualCommit | cut -c-10`;
        clean_up_jenkins
fi
echo $sha_tag > VERSION.txt

# Use shipyard to generate Makefile
bash ./bin/shipyard.sh
# Build Kubernetes containers first, travis will be waiting
make -j -f CI_build.Makefile kubernetes_test_containers
# Build the rest of the containers
make -j -f CI_build.Makefile all

# Run all test
make -j10 -f CI_test.Makefile all