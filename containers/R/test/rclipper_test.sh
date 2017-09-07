#!/usr/bin/env bash

set -e
set -u
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

version_tag=$(<VERSION.txt)

CLIPPER_ROOT=$DIR/../../..
cd $CLIPPER_ROOT_DIR

time docker build -t clipper/r-container-base:$version_tag -f ./RContainerDockerfile ./

USER_PACKAGE_DIR=$DIR/..
cd $USER_PACKAGE_DIR

R CMD INSTALL rclipper_user

cd $DIR

Rscript build_test_model.R

python deploy_query_test_model.py
