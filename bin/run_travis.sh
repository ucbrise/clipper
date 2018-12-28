#!/usr/bin/env bash

set -x
set -e
set -u
set -o pipefail

# Set up test environment
sha_tag=$(git rev-parse --verify --short=10 HEAD)
echo $sha_tag > VERSION.txt
export CLIPPER_REGISTRY=clippertesting

# Wait for all kubernetes specific images to be built in travis
make -j wait_for_kubernetes_test_containers

# Run the following test in sequence
cd integration-tests
time python kubernetes_integration_test.py
time python kubernetes_multi_frontend.py
time python kubernetes_namespace.py
time python multi_tenancy_test.py --kubernetes

# TODO: disabled for now, will re-enable after RBAC PR
# time python clipper_metric_kube.py