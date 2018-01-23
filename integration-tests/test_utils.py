from __future__ import absolute_import, division, print_function
import os
import sys
import pprint
import random
import socket
import docker
import logging
import time
import tempfile
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin import ClipperConnection, DockerContainerManager, KubernetesContainerManager, CLIPPER_TEMP_DIR
from clipper_admin.container_manager import CLIPPER_DOCKER_LABEL
from clipper_admin import __version__ as clipper_version

logger = logging.getLogger(__name__)

headers = {'Content-type': 'application/json'}
if not os.path.exists(CLIPPER_TEMP_DIR):
    os.makedirs(CLIPPER_TEMP_DIR)

fake_model_data = tempfile.mkdtemp(dir=CLIPPER_TEMP_DIR)


class BenchmarkException(Exception):
    def __init__(self, value):
        self.parameter = value

    def __str__(self):
        return repr(self.parameter)


# range of ports where available ports can be found
PORT_RANGE = [34256, 50000]


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
            # Make sure we clean up after binding
            del sock
            return port
        except socket.error as e:
            logger.info("Socket error: {}".format(e))
            logger.info(
                "randomly generated port %d is bound. Trying again." % port)


def create_docker_connection(cleanup=True, start_clipper=True):
    logging.info("Creating DockerContainerManager")
    cm = DockerContainerManager(
        clipper_query_port=find_unbound_port(),
        clipper_management_port=find_unbound_port(),
        clipper_rpc_port=find_unbound_port(),
        redis_port=find_unbound_port())
    cl = ClipperConnection(cm)
    if cleanup:
        cl.stop_all()
        docker_client = get_docker_client()
        docker_client.containers.prune(filters={"label": CLIPPER_DOCKER_LABEL})
    if start_clipper:
        # Try to start Clipper in a retry loop here to address flaky tests
        # as described in https://github.com/ucbrise/clipper/issues/352
        while True:
            try:
                logging.info("Starting Clipper")
                cl.start_clipper()
                time.sleep(1)
                break
            except docker.errors.APIError as e:
                logging.info(
                    "Problem starting Clipper: {}\nTrying again.".format(e))
                cl.stop_all()
                cm = DockerContainerManager(
                    clipper_query_port=find_unbound_port(),
                    clipper_management_port=find_unbound_port(),
                    clipper_rpc_port=find_unbound_port(),
                    redis_port=find_unbound_port())
                cl = ClipperConnection(cm)
    else:
        cl.connect()
    return cl


def create_kubernetes_connection(cleanup=True, start_clipper=True):
    logging.info("Creating KubernetesContainerManager")
    kubernetes_ip = "https://api.cluster.clipper-k8s-testing.com"
    logging.info("Kubernetes IP: %s" % kubernetes_ip)
    cm = KubernetesContainerManager(kubernetes_ip)
    cl = ClipperConnection(cm)
    if cleanup:
        cl.stop_all()
        # Give kubernetes some time to clean up
        time.sleep(20)
    if start_clipper:
        logging.info("Starting Clipper")
        cl.start_clipper(
            query_frontend_image=
            "568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/query_frontend:{}".
            format(clipper_version),
            mgmt_frontend_image=
            "568959175238.dkr.ecr.us-west-1.amazonaws.com/clipper/management_frontend:{}".
            format(clipper_version))
        time.sleep(1)
    else:
        try:
            cl.connect()
        except Exception as e:
            pass
    return cl


def log_clipper_state(cl):
    pp = pprint.PrettyPrinter(indent=4)
    logger.info("\nAPPLICATIONS:\n{app_str}".format(
        app_str=pp.pformat(cl.get_all_apps(verbose=True))))
    logger.info("\nMODELS:\n{model_str}".format(
        model_str=pp.pformat(cl.get_all_models(verbose=True))))
    logger.info("\nCONTAINERS:\n{cont_str}".format(
        cont_str=pp.pformat(cl.get_all_model_replicas(verbose=True))))
