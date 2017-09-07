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

Rscript rclipper_test.R
if [ $? -neq 0 ]; then
	echo "Failed during execution of R test script"
	exit(1)
fi

python rclipper_test.py

if [ $? -neq 0]; then
	echo "Failed during execution of python test script"
	exit(1)
fi
