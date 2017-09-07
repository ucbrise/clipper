#!/usr/bin/env bash

set -e
set -u
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

version_tag=$(<VERSION.txt)

cd $DIR/..

time docker build -t clipper/r-container-base:$version_tag -f ./RContainerDockerfile ./

R CMD INSTALL rclipper_user

cd $DIR

Rscript build_test_model.R

python deploy_query_test_model.py
