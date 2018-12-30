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

print_debug_info() {
    # while true; do
        kubectl get pods | python ../bin/colorize_output.py --tag "kubectl pods" --no-color
        kubectl describe pods | python ../bin/colorize_output.py --tag "kubectl describe" --no-color
        # sleep 120
    # done
}

# if the test test succeed, debug info will not get printed
# mainly used to debug container being evicted
time python kubernetes_integration_test.py || (print_debug_info() && exit 1)
time python kubernetes_multi_frontend.py || (print_debug_info() && exit 1)
time python kubernetes_namespace.py || (print_debug_info() && exit 1)
time python multi_tenancy_test.py --kubernetes || (print_debug_info() && exit 1)

# TODO: disabled for now, will re-enable after RBAC PR
# time python clipper_metric_kube.py