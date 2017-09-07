#!/usr/bin/env bash

# This script builds all the Clipper Docker images and publishes the useful ones to
# Docker Hub (images that we don't publish are things like the unittest image and the
# shared dependency base that don't really make sense to distribute on their own).
# Each image that is built is tagged with two tags:
# + The current Git SHA: <image_name>:git_hash
# + The current version as read from VERSION.txt: <image_name>:version
# For the images that we publish, both tags will be pushed.


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

namespace="clipper"

# We build images with the SHA tag to try to prevent clobbering other images
# being built from different branches on the same machine. This is particularly
# useful for running these scripts on the Jenkins build cluster.
create_image () {
    local image=$1          # Param: The name of the image to build

    local dockerfile=$2     # the Dockerfile name within the
                            # <clipper_root>/dockerfiles directory

    local public=$3        # Push the built images to Docker Hub under
                            # the clipper namespace. Must have credentials.
                     
    echo "Building $namespace/$image:$sha_tag from file $dockerfile"
    time docker build --build-arg CODE_VERSION=$sha_tag -t $namespace/$image:$sha_tag \
        -f dockerfiles/$dockerfile $CLIPPER_ROOT
    docker tag $namespace/$image:$sha_tag $namespace/$image:$version_tag

    if [ "$publish" = true ] && [ "$public" = true ] ; then
        echo "Publishing $namespace/$image:$sha_tag"
        docker push $namespace/$image:$sha_tag
        echo "Publishing $namespace/$image:$version_tag"
        docker push $namespace/$image:$version_tag
    fi
}


# Build the Clipper Docker images.
build_images () {
    # True and false indicate whether or not to publish the image. We publish public images.
    local private=false
    local public=true

    ########################$$####### WARNING: #################################
    # Some of these images depend on each other prior images. Do not change
    # the order in which the images are built
    ###########################################################################

    # Build Clipper core images
    create_image lib_base ClipperLibBaseDockerfile $private
    create_image query_frontend QueryFrontendDockerfile $public
    create_image management_frontend ManagementFrontendDockerfile $public
    create_image unittests ClipperTestsDockerfile  $private

    # Build containers
    create_image spark-scala-container SparkScalaContainerDockerfile $public
    create_image r-container-base RContainerDockerfile $public

    # First build Python base image
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
  args="--nopublish"
else
  args=$1
fi

case $args in
    -p | --publish )        publish=true
                            build_images
                            ;;
    -n | --nopublish )      publish=false
                            build_images
                            ;;
    -h | --help )           usage
                            ;;
    * )                     usage
esac
