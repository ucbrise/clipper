#!/usr/bin/env bash

set -x
set -e
set -u
set -o pipefail

# Set up test environment
sha_tag=$(git rev-parse --verify --short=10 HEAD)
# If the variable is unset or equal to empty string -> skip
if [ -z ${TRAVIS_PULL_REQUEST_SHA+x} ] || [ -z "$TRAVIS_PULL_REQUEST_SHA"]
    then echo "Not a travis PR Build"
    else sha_tag=`echo $TRAVIS_PULL_REQUEST_SHA | cut -c-10`
fi
echo $sha_tag > VERSION.txt

# We are still _pulling_ images from clippertesting
# The model container images will be pushed to localhost:5000
# as specified by
#   integration-tests/test_utils.py
#   39:CLIPPER_CONTAINER_REGISTRY = 'localhost:5000'
export CLIPPER_REGISTRY="clippertesting"

# Wait for all kubernetes specific images to be built in travis
# and retag them so we can use them in local registry. 
make -j -f CI_build.Makefile wait_for_kubernetes_test_containers

# Run the following test in sequence
cd integration-tests

print_debug_info() {
    kubectl get pods --all-namespaces | python ../bin/colorize_output.py --tag "kubectl pods" --no-color
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

k_delete_all() {
    kubectl delete deploy --all
    kubectl delete svc --all
    kubectl delete configmap --all
}

export TIMEOUT=10m

retry_test() {
    for i in {1..2}; do  
        timeout -s SIGINT $TIMEOUT $@ && break  \
        || (
            print_debug_info; 
            echo "failed at try $i, retrying"; 
            k_delete_all;
            if [ "$i" == "2" ];  
                then 
                    print_debug_info
                    exit 1; 
                fi; 
        )
    done
}

# if the test test succeed, debug info will not get printed
# mainly used to debug container being evicted
retry_test python kubernetes_integration_test.py; sleep 30
retry_test python kubernetes_multi_frontend.py; sleep 30
retry_test python kubernetes_namespace.py; sleep 30
retry_test python multi_tenancy_test.py --kubernetes; sleep 30
retry_test python clipper_metric_kube.py
