# This script will delete all testing containers and images
# available on this machine. 

CLIPPER_REGISTRY=clippertesting

# best effort, ignore all errors
try_cleanup() {
    (docker ps --all | grep "$@" | awk '{ print $1 }' | xargs docker kill) || true
    (docker container ls --all | grep "$@" | awk '{ print $3 }' | xargs docker container rm) || true
    (docker image ls --filter "label=maintainer=Dan Crankshaw <dscrankshaw@gmail.com>" | awk '{ print $3 }' | xargs docker image rm -f) || true
    docker image prune -f
}

try_cleanup_docker_volume() {
    (docker volume ls -f dangling=true | awk '{ print $2 }' | xargs docker volume rm -f) || true
}
try_cleanup ${CLIPPER_REGISTRY} > /dev/null
try_cleanup shipyard > /dev/null
try_cleanup_docker_volume > /dev/null
