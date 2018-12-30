CLIPPER_REGISTRY=clippertesting

# best effort, ignore all errors

(docker ps | grep $CLIPPER_REGISTRY | awk '{ print $1 }' | xargs docker kill) || true
(docker container ls | grep $CLIPPER_REGISTRY | awk '{ print $3 }' | xargs docker container rm) || true
(docker image ls | grep $CLIPPER_REGISTRY | awk '{ print $3 }' | xargs docker image rm -f) || true