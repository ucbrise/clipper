CLIPPER_REGISTRY=clippertesting

docker ps | grep $CLIPPER_REGISTRY | awk '{ print $1 }' | xargs docker kill
docker container ls | grep $CLIPPER_REGISTRY | awk '{ print $3 }' | xargs docker container rm
docker image ls | grep $CLIPPER_REGISTRY | awk '{ print $3 }' | xargs docker image rm