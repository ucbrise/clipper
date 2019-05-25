# This script will delete all testing containers and images
# available on this machine. 

CLIPPER_REGISTRY=clippertesting

# best effort, ignore all errors
try_cleanup() {
    (docker ps --all | grep "$@" | awk '{ print $1 }' | xargs docker kill) || true
    (docker container ls --all | grep "$@" | awk '{ print $3 }' | xargs docker container rm) || true
    (docker image ls --all | grep "$@" | awk '{ print $3 }' | xargs docker image rm -f) || true
}

try_cleanup CLIPPER_REGISTRY > /dev/null
try_cleanup shipyard > /dev/null
