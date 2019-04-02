from __future__ import print_function
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys


if __name__ == '__main__':
    clipper_conn = ClipperConnection(DockerContainerManager())
 #   python_deployer.create_endpoint(clipper_conn, "simple-example", "doubles",
 #                                   feature_sum)


    clipper_conn.stop_all()
    time.sleep(2)

