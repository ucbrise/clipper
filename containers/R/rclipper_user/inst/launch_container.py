import sys
import os
import argparse

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.abspath(cur_dir), "../../../../clipper_admin_v2/"))

from clipper_admin import deploy_model

sys.path.insert(0, os.path.join(os.path.abspath(cur_dir), "../../../../clipper_admin_v2/clipper_admin/"))
from docker import docker_container_manager

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description="Launch an R model container")
	parser.add_argument("-i", "--clipper_ip", type=str, nargs="+", help="The ip address of a clipper host machine")

	args = parser.parse_args()
	if not args.clipper_ip:
		print("The ip address of a clipper host must be specified!")
		raise

	clipper_ip = args.clipper_ip[0]
