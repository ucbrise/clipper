#!/usr/bin/env bash

# This script builds all the Clipper Docker images and publishes the useful ones to
# Docker Hub (images that we don't publish are things like the unittest image and the
# shared dependency base that don't really make sense to distribute on their own).
# Each image that is built is tagged with two tags:
# + The current Git SHA: <image_name>:git_hash
# + The current version as read from VERSION.txt: <image_name>:version
# For the images that we publish, both tags will be pushed.

# In addition, if we are on a release branch (one that matches the regex "release-*")
# and the version in VERSION.txt is not a release candidate and matches the form
# MAJOR.MINOR.PATCH, we will publish an additional tag MAJOR.MINOR. This allows users
# pin their docker images to the minor version and get updates with new patches
# automatically.

set -x
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
sha_tag=`git rev-parse --verify --short=10 HEAD`

######## Utilities for managing versioning ############
# From https://github.com/cloudflare/semver_bash/blob/c1133faf0efe17767b654b213f212c326df73fa3/semver.sh
# LICENSE: 
# Copyright (c) 2013, Ray Bejjani
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met: 
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer. 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation are those
# of the authors and should not be interpreted as representing official policies, 
# either expressed or implied, of the FreeBSD Project.

function semverParseInto() {
    local RE='[^0-9]*\([0-9]*\)[.]\([0-9]*\)[.]\([0-9]*\)\([0-9A-Za-z-]*\)'
    #MAJOR
    eval $2=`echo $1 | sed -e "s#$RE#\1#"`
    #MINOR
    eval $3=`echo $1 | sed -e "s#$RE#\2#"`
    #MINOR
    eval $4=`echo $1 | sed -e "s#$RE#\3#"`
    #SPECIAL
    eval $5=`echo $1 | sed -e "s#$RE#\4#"`
}

function semverEQ() {
    local MAJOR_A=0
    local MINOR_A=0
    local PATCH_A=0
    local SPECIAL_A=0

    local MAJOR_B=0
    local MINOR_B=0
    local PATCH_B=0
    local SPECIAL_B=0

    semverParseInto $1 MAJOR_A MINOR_A PATCH_A SPECIAL_A
    semverParseInto $2 MAJOR_B MINOR_B PATCH_B SPECIAL_B

    if [ $MAJOR_A -ne $MAJOR_B ]; then
        return 1
    fi

    if [ $MINOR_A -ne $MINOR_B ]; then
        return 1
    fi

    if [ $PATCH_A -ne $PATCH_B ]; then
        return 1
    fi

    if [[ "_$SPECIAL_A" != "_$SPECIAL_B" ]]; then
        return 1
    fi


    return 0

}

function semverLT() {
    local MAJOR_A=0
    local MINOR_A=0
    local PATCH_A=0
    local SPECIAL_A=0

    local MAJOR_B=0
    local MINOR_B=0
    local PATCH_B=0
    local SPECIAL_B=0

    semverParseInto $1 MAJOR_A MINOR_A PATCH_A SPECIAL_A
    semverParseInto $2 MAJOR_B MINOR_B PATCH_B SPECIAL_B

    if [ $MAJOR_A -lt $MAJOR_B ]; then
        return 0
    fi

    if [[ $MAJOR_A -le $MAJOR_B  && $MINOR_A -lt $MINOR_B ]]; then
        return 0
    fi
    
    if [[ $MAJOR_A -le $MAJOR_B  && $MINOR_A -le $MINOR_B && $PATCH_A -lt $PATCH_B ]]; then
        return 0
    fi

    if [[ "_$SPECIAL_A"  == "_" ]] && [[ "_$SPECIAL_B"  == "_" ]] ; then
        return 1
    fi
    if [[ "_$SPECIAL_A"  == "_" ]] && [[ "_$SPECIAL_B"  != "_" ]] ; then
        return 1
    fi
    if [[ "_$SPECIAL_A"  != "_" ]] && [[ "_$SPECIAL_B"  == "_" ]] ; then
        return 0
    fi

    if [[ "_$SPECIAL_A" < "_$SPECIAL_B" ]]; then
        return 0
    fi

    return 1

}

function semverGT() {
    semverEQ $1 $2
    local EQ=$?

    semverLT $1 $2
    local LT=$?

    if [ $EQ -ne 0 ] && [ $LT -ne 0 ]; then
        return 0
    else
        return 1
    fi
}


##############################################################

set_version_tag () {
  if ! [[ "$version_tag" == "develop" ]] ; then
    local MAJOR=0
    local MINOR=0
    local PATCH=0
    local SPECIAL=""
    semverParseInto $version_tag MAJOR MINOR PATCH SPECIAL
    if [[ -z "$SPECIAL"  ]] ; then
      minor_version="$MAJOR.$MINOR"
    else
      echo "special found"
    fi
  fi
}


set_version_tag

namespace=$(docker info | grep Username | awk '{ print $2 }')

# Clear clipper_docker_images.txt for future write
rm -f $CLIPPER_ROOT/bin/clipper_docker_images.txt
touch $CLIPPER_ROOT/bin/clipper_docker_images.txt

# We build images with the SHA tag to try to prevent clobbering other images
# being built from different branches on the same machine. This is particularly
# useful for running these scripts on the Jenkins build cluster.
create_image () {
    local image=$1          # Param: The name of the image to build

    local dockerfile=$2     # the Dockerfile name within the
                            # <clipper_root>/dockerfiles directory

    local public=$3        # Push the built images to Docker Hub under
                            # the clipper namespace. Must have credentials.

    if [ "$#" -eq 4 ]; then
      local rpc_version="--build-arg RPC_VERSION=$4"
    else
      local rpc_version=""
    fi

                     
    echo "Building $namespace/$image:$sha_tag from file $dockerfile"
    time docker build --build-arg CODE_VERSION=$sha_tag --build-arg REGISTRY=$namespace $rpc_version -t $namespace/$image:$sha_tag \
        -f dockerfiles/$dockerfile $CLIPPER_ROOT

    echo "Image tag appended to CLIPPER_ROOT/bin/clipper_docker_images.txt"
    echo "$namespace/$image:$sha_tag" >> $CLIPPER_ROOT/bin/clipper_docker_images.txt

    if [ "$publish" = true ] && [ "$public" = true ] ; then
        docker tag $namespace/$image:$sha_tag $namespace/$image:$version_tag

        echo "Publishing $namespace/$image:$sha_tag"
        docker push $namespace/$image:$sha_tag
        echo "Publishing $namespace/$image:$version_tag"
        docker push $namespace/$image:$version_tag

        # If the version is normal versioned release (not develop and not a release candidate),
        # We also tag and publish an image tagged
        # with just the minor version. E.g. if VERSION.txt is "0.2.0", we'll also
        # publish an image tagged with "0.2". This image will be updated to the newest
        # patch version every time we push a patch, but will not be updated for release
        # candidates.
        if ! [[ -z ${minor_version+set} ]] ; then
          docker tag $namespace/$image:$sha_tag $namespace/$image:$minor_version
          echo "Found release version. Publishing $namespace/$image:$minor_version"
          docker push $namespace/$image:$minor_version
        fi
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

    # Build the rest in parallel
    create_image query_frontend QueryFrontendDockerfile $public &
    create_image management_frontend ManagementFrontendDockerfile $public &
    create_image dev ClipperDevDockerfile  $public &
    create_image py35-dev ClipperPy35DevDockerfile  $public &
    wait

    # The test images depend on the dev images, so wait until
    # the dev images have been built.

    create_image unittests ClipperTestsDockerfile  $private &
    create_image py35tests ClipperPy35TestsDockerfile  $private &
    wait

    # Build containers for other languages
    create_image spark-scala-container SparkScalaContainerDockerfile $public &
    create_image r-container-base RContainerDockerfile $public &

    # First build Python base image
    create_image py-rpc Py2RPCDockerfile $public &
    create_image py35-rpc Py35RPCDockerfile $public &
    create_image py36-rpc Py36RPCDockerfile $public &
    wait

    create_image sum-container SumDockerfile  $private &
    create_image noop-container NoopDockerfile $public &

    create_image python-closure-container PyClosureContainerDockerfile $public py &
    create_image python35-closure-container PyClosureContainerDockerfile $public py35 &
    create_image python36-closure-container PyClosureContainerDockerfile $public py36 &
    wait

    create_image pyspark-container PySparkContainerDockerfile $public py &
    create_image pyspark35-container PySparkContainerDockerfile $public py35 &
    create_image pyspark36-container PySparkContainerDockerfile $public py36 &

    create_image tf-container TensorFlowDockerfile $public py &
    create_image tf35-container TensorFlowDockerfile $public py35 &
    create_image tf36-container TensorFlowDockerfile $public py36 &
    wait

    create_image pytorch-container PyTorchContainerDockerfile $public py &
    create_image pytorch35-container PyTorchContainerDockerfile $public py35 &
    create_image pytorch36-container PyTorchContainerDockerfile $public py36 &

    # See issue #475
    # create_image caffe2-onnx-container Caffe2OnnxDockerfile $public py
    # create_image caffe235-onnx-container Caffe2OnnxDockerfile $public py35
    # create_image caffe236-onnx-container Caffe2OnnxDockerfile $public py36

    create_image mxnet-container MXNetContainerDockerfile $public py &
    create_image mxnet35-container MXNetContainerDockerfile $public py35 &
    create_image mxnet36-container MXNetContainerDockerfile $public py36 &
    wait

    # Build Metric Monitor image - no dependency
    create_image frontend-exporter FrontendExporterDockerfile $public
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
