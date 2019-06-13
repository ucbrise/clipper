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
unset do_cleanup

# Change the VERSION.txt to current sha_tag
sha_tag=$(git rev-parse --verify --short=10 HEAD)
# Jenkins will merge the PR, however we will use the unmerged
# sha to tag our image. 
# https://stackoverflow.com/questions/3601515/how-to-check-if-a-variable-is-set-in-bash
if [ -z ${ghprbActualCommit+x} ]
    then echo "We are not doing Jekins PRB"
    else 
        sha_tag=`echo $ghprbActualCommit | cut -c-10`;
        clean_up_jenkins
        do_cleanup="true"
fi
echo $sha_tag > VERSION.txt

if [ -z ${BUILD_TAG+x} ]
    then echo "We are not doing Jekins Regular Build"
    else 
        clean_up_jenkins
        do_cleanup="true"
fi

# Use shipyard to generate Makefile
bash ./bin/shipyard.sh
# Build Kubernetes containers first, travis will be waiting
make -j -f CI_build.Makefile kubernetes_test_containers
# Build the rest of the containers
make -j -f CI_build.Makefile all

# Run all test
make -j10 -f CI_test.Makefile unittest_py2
make -j15 -f CI_test.Makefile integration_py2
make -j15 -f CI_test.Makefile integration_py36

# NOTE: We will use these tests after Jan. 1, 2020. (python 2.7 will be retired.)
# We have to active 'py37_dev' and 'py37tests' codes in clipper_docker.cfg.py also.
# make -j10 -f CI_test.Makefile unittest_py36
# make -j15 -f CI_test.Makefile integration_py36
# make -j15 -f CI_test.Makefile integration_py37


if [ -z ${do_cleanup+x} ]
    then clean_up_jenkins
fi
