#!/usr/bin/env bash

set -e
set -u
set -o pipefail

redis_port=$1

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $DIR

version_tag=$(<VERSION.txt)

CLIPPER_ROOT=$DIR/../..
cd $CLIPPER_ROOT

time docker build -t clipper/r-container-base:$version_tag -f ./dockerfiles/RContainerDockerfile ./

USER_PACKAGE_DIR=$CLIPPER_ROOT/containers/R
cd $USER_PACKAGE_DIR

R CMD INSTALL rclipper_user

cd $DIR

Rscript build_test_model.R

python deploy_query_test_model.py $redis_port
