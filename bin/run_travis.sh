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
    kubectl get pods | python ../bin/colorize_output.py --tag "kubectl pods" --no-color
    kubectl describe pods | python ../bin/colorize_output.py --tag "kubectl describe" --no-color
}

# Print it periodically to help debug getting stuck
print_debug_info_periodic() {
    while true; do
        kubectl get pods | python ../bin/colorize_output.py --tag "kubectl pods" --no-color
        sleep 120
    done
}
print_debug_info_periodic&

export NUM_RETRIES=2

retry_test() {
    for i in {1..$NUM_RETRIES}; do  
        (timeout -s SIGINT 5m "$@" && break) || (print_debug_info; echo "failed at try $i, retrying")
    if [ "$i" -eq "$NUM_RETRIES" ];  
        then 
            print_debug_info
            exit 1; 
        fi; 
    done
}
# if the test test succeed, debug info will not get printed
# mainly used to debug container being evicted
retry_test "python kubernetes_integration_test.py"
retry_test "python kubernetes_multi_frontend.py"
retry_test "python kubernetes_namespace.py"
retry_test "python multi_tenancy_test.py --kubernetes"

# TODO: disabled for now, will re-enable after RBAC PR
# time python clipper_metric_kube.py