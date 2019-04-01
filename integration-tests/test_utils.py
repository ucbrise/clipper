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
from clipper_admin import (ClipperConnection, DockerContainerManager,
                           KubernetesContainerManager, CLIPPER_TEMP_DIR,
                           ClipperException)
from clipper_admin.container_manager import CLIPPER_DOCKER_LABEL

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

# The dockerhub account we are pushing kubernetes built images to
# Here we are assuming localhost:5000 is running docker registry.
CLIPPER_CONTAINER_REGISTRY = 'localhost:5000'

# USE_MINIKUBE == True -> useInternalIP = True
USE_MINIKUBE = True


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


def create_docker_connection(cleanup=False,
                             start_clipper=False,
                             cleanup_name='default-cluster',
                             new_name='default-cluster'):
    logger.info("Creating DockerContainerManager")
    cl = None
    assert cleanup or start_clipper, "You must set at least one of {cleanup, start_clipper} to be true."

    if cleanup:
        logger.info("Cleaning up Docker cluster {}".format(cleanup_name))
        cm = DockerContainerManager(
            cluster_name=cleanup_name,
            clipper_query_port=find_unbound_port(),
            clipper_management_port=find_unbound_port(),
            clipper_rpc_port=find_unbound_port(),
            redis_port=find_unbound_port(),
        )
        cl = ClipperConnection(cm)
        cl.stop_all(graceful=False)

    if start_clipper:
        # Try to start Clipper in a retry loop here to address flaky tests
        # as described in https://github.com/ucbrise/clipper/issues/352
        logger.info("Starting up Docker cluster {}".format(new_name))

        while True:
            cm = DockerContainerManager(
                cluster_name=new_name,
                clipper_query_port=find_unbound_port(),
                clipper_management_port=find_unbound_port(),
                clipper_rpc_port=find_unbound_port(),
                redis_port=find_unbound_port())
            cl = ClipperConnection(cm)
            try:
                logger.info("Starting Clipper")
                cl.start_clipper()
                time.sleep(1)
                break
            except docker.errors.APIError as e:
                logger.info(
                    "Problem starting Clipper: {}\nTrying again.".format(e))
                cl.stop_all()
    return cl


def create_kubernetes_connection(cleanup=False,
                                 start_clipper=False,
                                 connect=False,
                                 with_proxy=False,
                                 num_frontend_replicas=1,
                                 cleanup_name='default-cluster',
                                 new_name='default-cluster',
                                 connect_name='default-cluster',
                                 namespace='default'):
    logger.info("Creating KubernetesContainerManager")
    cl = None
    assert cleanup or start_clipper or connect, "You must set at least one of {cleanup, start_clipper, connect} to be true."

    if with_proxy:
        kubernetes_proxy_addr = "127.0.0.1:8080"
    else:
        kubernetes_proxy_addr = None

    if cleanup:
        logger.info("Cleaning up Kubernetes Cluster {}".format(cleanup_name))
        cm = KubernetesContainerManager(
            cluster_name=cleanup_name,
            useInternalIP=USE_MINIKUBE,
            kubernetes_proxy_addr=kubernetes_proxy_addr)
        cl = ClipperConnection(cm)
        cl.stop_all()
        logger.info("Done cleaning up clipper")

    if start_clipper:
        logger.info("Starting up Kubernetes Cluster {}".format(new_name))
        cm = KubernetesContainerManager(
            cluster_name=new_name,
            kubernetes_proxy_addr=kubernetes_proxy_addr,
            namespace=namespace,
            useInternalIP=USE_MINIKUBE,
            create_namespace_if_not_exists=True)
        cl = ClipperConnection(cm)
        cl.start_clipper(num_frontend_replicas=num_frontend_replicas)

    if connect:
        try:
            cm = KubernetesContainerManager(
                cluster_name=connect_name,
                useInternalIP=USE_MINIKUBE,
                kubernetes_proxy_addr=kubernetes_proxy_addr)
            cl = ClipperConnection(cm)
            cl.connect()
        except Exception:
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


def log_docker(clipper_conn):
    if clipper_conn is None:
        return

    """Retrieve status and log for last ten containers"""
    container_runing = clipper_conn.cm.docker_client.containers.list(limit=10)
    logger.info('----------------------')
    logger.info('Last ten containers status')
    for cont in container_runing:
        logger.info('Name {}, Image {}, Status {}, Label {}'.format(
            cont.name, cont.image, cont.status, cont.labels))

    logger.info('----------------------')
    logger.info('Printing out logs')

    for cont in container_runing:
        logger.info('Name {}, Image {}, Status {}, Label {}'.format(
            cont.name, cont.image, cont.status, cont.labels))
        logger.info(cont.logs())
