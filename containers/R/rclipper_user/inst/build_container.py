import sys
import os
import argparse

from clipper_admin import DockerContainerManager, ClipperConnection, ClipperException
from clipper_admin import version

CLIPPER_R_CONTAINER_BASE_IMAGE = "{}/r-container-base:{}".format(
    version.__registry__, version.__version__)

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
        help=
        "The Docker container registry to which to push the freshly built model image"
    )

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

    # Note: This container manager is only necessary for
    # creating a connection object that can be used to build the model
    cm = DockerContainerManager()

    conn = ClipperConnection(cm)

    conn.build_model(args.model_name, args.model_version, args.model_data_path,
                     CLIPPER_R_CONTAINER_BASE_IMAGE, args.registry)
