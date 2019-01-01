#!/usr/bin/env bash

set -x
set -e
set -u
set -o pipefail


run_all=$1

function clean_up {
    # Clean up credentials
    rm $KUBECONFIG
    exit
}


unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
cd $DIR/..
tag=$(<VERSION.txt)
cd -

# Log in to Kubernetes Docker repo
# $DIR/aws_docker_repo_login.sh

# Test docker login
# docker pull 568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:$tag

# Set up credentials for K8s testing cluster.
export KUBECONFIG=~/kubeconfig_$(date +"%Y%m%d%H%M%S")

# We only need to trap exit starting here, because before this line
# KUBECONFIG hasn't been set yet and we haven't created any resources
# that need to be cleaned up.
trap clean_up SIGHUP SIGINT SIGTERM EXIT

# NOTE: This script will fail if the following environment variables are not set
# + CLIPPER_K8S_CERT_AUTH
# + CLIPPER_K8S_CLIENT_CERT
# + CLIPPER_K8S_CLIENT_KEY
# + CLIPPER_K8S_PASSWORD
python $DIR/construct_kube_config.py $KUBECONFIG

# Test K8s cluster access
kubectl get nodes
# Set kubectl proxy for k8s tests later
kubectl proxy --port 8080 &

# Login to clippertesting dockerhub here
docker login --username="clippertesting" --password=$CLIPPER_TESTING_DOCKERHUB_PASSWORD

if [[ $run_all = "true" ]]; then
    $DIR/check_format.sh
    $DIR/run_unittests.sh
else
    $DIR/run_unittests.sh -i
fi
