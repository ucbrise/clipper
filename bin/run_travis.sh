#!/usr/bin/env bash

set -x
set -e
set -u
set -o pipefail

sha_tag=$(git rev-parse --verify --short=10 HEAD)
echo $sha_tag > VERSION.txt
make -j6 wait_for_kubernetes_test_containers
cd integration-tests

export CLIPPER_REGISTRY=clippertesting
python kubernetes_integration_test.py
python kubernetes_multi_frontend.py
python kubernetes_namespace.py
python multi_tenancy_test.py --kubernetes