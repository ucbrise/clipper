import sys
import os
import argparse
import docker
import tempfile
import re
import tarfile
import six

from cl_exceptions import ClipperException

from version import __version__

deploy_regex_str = "[a-z0-9]([-a-z0-9]*[a-z0-9])?\Z"
deployment_regex = re.compile(deploy_regex_str)

CLIPPER_R_CONTAINER_BASE_IMAGE = "clipper/r-container-base:{}".format(__version__)

def validate_versioned_model_name(name, version):
    if deployment_regex.match(name) is None:
        raise ClipperException(
            "Invalid value: {name}: a model name must be a valid DNS-1123 "
            " subdomain. It must consist of lower case "
            "alphanumeric characters, '-' or '.', and must start and end with "
            "an alphanumeric character (e.g. 'example.com', regex used for "
            "validation is '{reg}'".format(name=name, reg=deploy_regex_str))
    if deployment_regex.match(version) is None:
        raise ClipperException(
            "Invalid value: {version}: a model version must be a valid DNS-1123 "
            " subdomain. It must consist of lower case "
            "alphanumeric characters, '-' or '.', and must start and end with "
            "an alphanumeric character (e.g. 'example.com', regex used for "
            "validation is '{reg}'".format(
                version=version, reg=deploy_regex_str))

def build_model(name,
                version,
                model_data_path,
                base_image,
                container_registry=None):
    """Build a new model container Docker image with the provided data"

    This method builds a new Docker image from the provided base image with the local directory specified by
    ``model_data_path`` copied into the image. The Dockerfile that gets generated to build the image
    is equivalent to the following::

        FROM <base_image>
        COPY <model_data_path> /model/

    The newly built image is then pushed to the specified container registry. If no container registry
    is specified, the image will be pushed to the default DockerHub registry. Clipper will tag the
    newly built image with the tag [<registry>]/<name>:<version>.

    This method can be called without being connected to a Clipper cluster.

    Parameters
    ----------
    name : str
        The name of the deployed model.
    version : str
        The version to assign this model. Versions must be unique on a per-model
        basis, but may be re-used across different models.
    model_data_path : str
        A path to a local directory. The contents of this directory will be recursively copied into the
        Docker container.
    base_image : str
        The base Docker image to build the new model image from. This
        image should contain all code necessary to run a Clipper model
        container RPC client.
    container_registry : str, optional
        The Docker container registry to push the freshly built model to. Note
        that if you are running Clipper on Kubernetes, this registry must be accesible
        to the Kubernetes cluster in order to fetch the container from the registry.

    Returns
    -------
    str :
        The fully specified tag of the newly built image. This will include the
        container registry if specified.

    Raises
    ------
    :py:exc:`clipper.ClipperException`

    Note
    ----
    Both the model name and version must be valid DNS-1123 subdomains. Each must consist of
    lower case alphanumeric characters, '-' or '.', and must start and end with an alphanumeric
    character (e.g. 'example.com', regex used for validation is
    '[a-z0-9]([-a-z0-9]*[a-z0-9])?\Z'.
    """

    version = str(version)

    validate_versioned_model_name(name, version)

    with tempfile.NamedTemporaryFile(
            mode="w+b", suffix="tar") as context_file:
        # Create build context tarfile
        with tarfile.TarFile(
                fileobj=context_file, mode="w") as context_tar:
            context_tar.add(model_data_path)
            # From https://stackoverflow.com/a/740854/814642
            df_contents = six.StringIO(
                "FROM {container_name}\nCOPY {data_path} /model/\n".format(
                    container_name=base_image, data_path=model_data_path))
            df_tarinfo = tarfile.TarInfo('Dockerfile')
            df_contents.seek(0, os.SEEK_END)
            df_tarinfo.size = df_contents.tell()
            df_contents.seek(0)
            context_tar.addfile(df_tarinfo, df_contents)
        # Exit Tarfile context manager to finish the tar file
        # Seek back to beginning of file for reading
        context_file.seek(0)
        image = "{name}:{version}".format(name=name, version=version)
        if container_registry is not None:
            image = "{reg}/{image}".format(
                reg=container_registry, image=image)
        docker_client = docker.from_env()
        print("Building model Docker image with model data from {}".
                    format(model_data_path))
        docker_client.images.build(
            fileobj=context_file, custom_context=True, tag=image)

    print("Pushing model Docker image to {}".format(image))
    docker_client.images.push(repository=image)
    return image

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Launch an R model container")
    parser.add_argument(
        "-n",
        "--model_name",
        type=str,
        help="The name of the model to be deployed")
    parser.add_argument(
        "-v",
        "--model_version",
        type=int,
        help="The version of the model to be deployed")
    parser.add_argument(
        "-m",
        "--model_data_path",
        type=str,
        help="The path to the serialized R model and associated data")
    parser.add_argument(
        "-r",
        "--registry",
        type=str,
        help="The Docker container registry to which to push the freshly built model image")

    args = parser.parse_args()
    arg_errs = []

    if not args.model_name:
        arg_errs.append(
            "The name of the model being deployed must be specified!")
    if not args.model_version:
        arg_errs.append(
            "The version of the model being deployed must be specified!")
    if not args.model_data_path:
        arg_errs.append(
            "The path to the serialized R model data must be specified!")

    if len(arg_errs) > 0:
        for err in arg_errs:
            print(err)
        raise ClipperException()

    build_model(args.model_name, args.model_version, args.model_data_path, CLIPPER_R_CONTAINER_BASE_IMAGE, args.registry)
