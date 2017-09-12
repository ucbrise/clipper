#!/usr/bin/env bash

set -e
set -u
set -o pipefail

redis_port=$1

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CLIPPER_ROOT=$DIR/../..

USER_PACKAGE_DIR=$CLIPPER_ROOT/containers/R
cd $USER_PACKAGE_DIR

R -e "install.packages('versions', repos='http://cran.us.r-project.org')"
R -e "versions::install.versions('CodeDepends', version='0.5-3')"

R CMD INSTALL rclipper_user

cd $DIR

Rscript build_test_model.R

python deploy_query_test_model.py $redis_port
