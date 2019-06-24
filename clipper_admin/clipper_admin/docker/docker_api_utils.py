from __future__ import absolute_import, division, print_function

import logging
import docker.errors
from urllib3.exceptions import TimeoutError
from requests.exceptions import Timeout
from ..decorators import retry
from ..exceptions import ClipperException


logger = logging.getLogger(__name__)


def create_network(docker_client, name):
    try:
        docker_client.networks.create(
            name=name,
            check_duplicate=True)
    except docker.errors.APIError:
        logger.debug(
            "{nw} network already exists".format(nw=name))
    except ConnectionError:
        msg = "Unable to Connect to Docker. Please Check if Docker is running."
        raise ClipperException(msg)


# Wait for maximum 5 min.
@retry((docker.errors.NotFound, docker.errors.APIError, ClipperException),
       tries=300, delay=1, backoff=1, logger=logger)
def check_container_status(docker_client, name):
    state = docker_client.containers.get(name).attrs.get("State")
    inspected = docker_client.api.inspect_container(name)
    if (state is not None and state.get("Status") == "running") or \
            (inspected is not None and inspected.get("State").get("Health").get(
                "Status") == "healthy"):
        return
    else:
        msg = "{} container is not running yet or broken. ".format(name) + \
              "We will try to run again. Please analyze logs if " + \
              "it keeps failing"
        raise ClipperException(msg)


@retry((docker.errors.APIError, TimeoutError, Timeout),
       tries=5, logger=logger)
def list_containers(docker_client, filters):
    return docker_client.containers.list(filters=filters)


@retry((docker.errors.APIError, TimeoutError, Timeout),
       tries=5, logger=logger)
def run_container(docker_client, image, cmd=None, name=None, ports=None,
                  labels=None, environment=None, log_config=None, volumes=None,
                  user=None, extra_container_kwargs=None):
    return docker_client.containers.run(
        image,
        command=cmd,
        name=name,
        ports=ports,
        labels=labels,
        environment=environment,
        volumes=volumes,
        user=user,
        log_config=log_config,
        **extra_container_kwargs)
