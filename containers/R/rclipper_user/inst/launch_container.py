import sys
import os
import argparse

cur_dir = os.path.dirname(os.path.abspath(__file__))

# DO NOT MERGE: For testing, change this to the path to clipper_admin_v2 on your system
sys.path.insert(
    0,
    "/Users/Corey/Documents/RISE/clipper/containers/R/rclipper_user/inst/../../../../clipper_admin_v2/"
)

from clipper_admin.docker import docker_container_manager as dcm
from clipper_admin import ClipperConnection

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
        "-i",
        "--clipper_ip",
        type=str,
        help="The ip address of a clipper host machine")

    args = parser.parse_args()
    arg_errs = []

    if not args.clipper_ip:
        arg_errs.append("The ip address of a clipper host must be specified!")
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
        raise

    clipper_ip = args.clipper_ip[0]

    print("Connecting to Clipper...")

    cm = dcm.DockerContainerManager(clipper_ip)
    cl_conn = ClipperConnection(cm)

    # The correct clipper data input type will be resolved from the R model sample input
    # at run time, so this field is only being included to meet API requirements
    input_type = "UNUSED"
    image_name = "clipper/r_container"

    print("Building image and deploying model...")

    cl_conn.deploy_model(args.model_name, args.model_version, input_type,
                         args.model_data_path, image_name)

    print("Done!")
