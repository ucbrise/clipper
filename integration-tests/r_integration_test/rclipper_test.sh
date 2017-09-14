#!/usr/bin/env bash

set -e
set -u
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CLIPPER_ROOT=$DIR/../..

USER_PACKAGE_DIR=$CLIPPER_ROOT/containers/R
cd $USER_PACKAGE_DIR

R CMD INSTALL rclipper_user

cd $DIR

pip install -e $CLIPPER_ROOT/clipper_admin

python deploy_query_test_model.py
