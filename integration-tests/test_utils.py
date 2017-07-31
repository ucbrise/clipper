from __future__ import absolute_import, division, print_function
import os
import sys
import pprint
import random
import socket
import docker
import logging
import time
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin_v2" % cur_dir))
from clipper_admin import ClipperConnection, DockerContainerManager, K8sContainerManager
from clipper_admin.container_manager import CLIPPER_DOCKER_LABEL

if sys.version < '3':
    import subprocess32 as subprocess
    PY3 = False
else:
    import subprocess
    PY3 = True

# SERVICE = "docker"
SERVICE = "k8s"

logger = logging.getLogger(__name__)

headers = {'Content-type': 'application/json'}
fake_model_data = "/tmp/test123456"
try:
    os.mkdir(fake_model_data)
except OSError:
    pass


class BenchmarkException(Exception):
    def __init__(self, value):
        self.parameter = value

    def __str__(self):
        return repr(self.parameter)


# range of ports where available ports can be found
PORT_RANGE = [34256, 40000]


def get_docker_client():
    if "DOCKER_API_VERSION" in os.environ:
        return docker.from_env(version=os.environ["DOCKER_API_VERSION"])
    else:
        return docker.from_env()


def find_unbound_port():
    """
    Returns an unbound port number on 127.0.0.1.
    """
    while True:
        port = random.randint(*PORT_RANGE)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.bind(("127.0.0.1", port))
            return port
        except socket.error:
            logger.debug(
                "randomly generated port %d is bound. Trying again." % port)


def create_connection(service, cleanup=True, start_clipper=True):
    if service == "docker":
        # TODO: create registry
        logging.info("Creating DockerContainerManager")
        cm = DockerContainerManager(
            "localhost", redis_port=find_unbound_port())
        cl = ClipperConnection(cm)
        if cleanup:
            cl.stop_all()
            docker_client = get_docker_client()
            docker_client.containers.prune(
                filters={"label": CLIPPER_DOCKER_LABEL})
    elif service == "k8s":
        logging.info("Creating K8sContainerManager")
        k8s_ip = subprocess.Popen(
            ['minikube', 'ip'],
            stdout=subprocess.PIPE).communicate()[0].strip()
        logging.info("K8s IP: %s" % k8s_ip)
        cm = K8sContainerManager(k8s_ip)
        cl = ClipperConnection(cm)
        if cleanup:
            cl.stop_all()
            # Give k8s some time to clean up
            time.sleep(10)
    else:
        msg = "{cm} is a currently unsupported container manager".format(
            cm=service)
        logging.error(msg)
        raise BenchmarkException(msg)
    if start_clipper:
        logging.info("Starting Clipper")
        cl.start_clipper()
        time.sleep(1)
    else:
        cl.connect()
    return cl


def log_clipper_state(cl):
    pp = pprint.PrettyPrinter(indent=4)
    logger.info("\nAPPLICATIONS:\n{app_str}".format(app_str=pp.pformat(
        cl.get_all_apps(verbose=True))))
    logger.info("\nMODELS:\n{model_str}".format(model_str=pp.pformat(
        cl.get_all_models(verbose=True))))
    logger.info("\nCONTAINERS:\n{cont_str}".format(cont_str=pp.pformat(
        cl.get_all_model_replicas(verbose=True))))
