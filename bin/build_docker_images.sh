#!/usr/bin/env bash

# This script builds all the public Docker images that the Clipper project distributes
# and pushes them to Docker hub.
# Each image is pushed with the tag <image_name>:version
# In addition, if the version is "develop", each image will also be
# tagged with <image_name>:git_hash. Git hash tags are only pushed for
# the develop branch.


set -e
set -u
set -o pipefail

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Let the user start this script from anywhere in the filesystem.
CLIPPER_ROOT=$DIR/..
cd $CLIPPER_ROOT

# Initialize tags
version_tag=$(<VERSION.txt)
sha_tag=`git rev-parse --verify --short HEAD`

# We build images with the SHA tag to try to prevent clobbering other images
# being built from different branches on the same machine. This is particularly
# useful for running these scripts on the Jenkins build cluster.
create_image () {
    local image=$1          # Param: The name of the image to build

    local dockerfile=$2     # the Dockerfile name within the
                            # <clipper_root>/dockerfiles directory

    local public=$3        # Push the built images to Docker Hub under
                            # the clipper namespace. Must have credentials.
                     
    time docker build --build-arg CODE_VERSION=$sha_tag -t clipper/$image:$sha_tag \
        -f dockerfiles/$dockerfile $CLIPPER_ROOT
    docker tag clipper/$image:$sha_tag clipper/$image:$version_tag

    if [ "$publish" = true && "$public" = true ] ; then
        docker push clipper/$image:$version_tag
        docker push clipper/$image:$sha_tag
    fi
}


# Build the Clipper Docker images.
build_images () {
    # True and false indicate whether or not to public the image. We public public images.
    local private=false
    local public=true

    ############## WARNING: ############
    # Some of these images depend on each other prior images.
    # You must preserve the build order.
    create_image lib_base ClipperLibBaseDockerfile $private
    create_image query_frontend QueryFrontendDockerfile $public
    create_image management_frontend ManagementFrontendDockerfile $public
    create_image unittests ClipperTestsDockerfile  $private


    # TODO TODO TODO: fix this centralize
    # Build Spark JVM Container
    cd $DIR/../containers/jvm
    time docker build -t clipper/spark-scala-container:$tag -f SparkScalaContainerDockerfile ./
    cd -

    # Build the Python model containers
    cd $DIR/..

    # first build base image
    create_image py-rpc RPCDockerfile $public
    create_image sum-container SumDockerfile  $private
    create_image noop-container NoopDockerfile $public
    create_image python-closure-container PyClosureContainerDockerfile $public
    create_image pyspark-container PySparkContainerDockerfile $public
    create_image tf_cifar_container TensorFlowCifarDockerfile $public
}

usage () {
    cat <<EOF
    usage: build docker_images.sh [--publish]

    Build and optionally publish the Clipper Docker images.

    Options:

    -p, --publish               Publish images to DockerHub
    -h, --help                  Display this message and exit.

$@
EOF
}


if [ "$#" == 0 ]
then
  publish=false
  build_images
else
  args=$1
fi

case $args in
    -p | --publish )        publish=true
                            build_images
                            ;;
    -h | --help )           usage
                            ;;
    * )                     usage
esac
