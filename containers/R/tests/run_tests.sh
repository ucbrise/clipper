#!/usr/bin/env bash

set -e
set -u
set -o pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $DIR

Rscript test_recursive_serialization.R
Rscript test_obj_dependent_serialization.R
Rscript test_lib_dependent_serialization.R
Rscript test_file_dependent_serialization.R
Rscript test_multidependency_serialization.R
