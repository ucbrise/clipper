from __future__ import absolute_import, division, print_function
import docker
import logging
import os
from ..container_manager import (
    ContainerManager, CLIPPER_QUERY_PORT, CLIPPER_MANAGEMENT_PORT,
    CLIPPER_RPC_PORT, CLIPPER_DOCKER_LABEL, CLIPPER_MODEL_CONTAINER_LABEL)

DOCKER_NETWORK_NAME = "clipper_network"

logger = logging.getLogger(__name__)


class DockerContainerManager(ContainerManager):
    def __init__(self,
                 clipper_public_hostname,
                 redis_ip=None,
                 redis_port=6379,
                 registry=None,
                 registry_username=None,
                 registry_password=None,
                 extra_container_kwargs={}):
        """
        Parameters
        ----------
        clipper_public_hostname : str
            The public hostname or IP address at which the Clipper Docker
            containers can be accessed via their exposed ports. On macOs this
            can be set to "localhost" as Docker automatically makes exposed
            ports on Docker containers available via localhost, but on other
            operating systems this must be set explicitly.

        extra_container_kwargs : dict
            Any additional keyword arguments to pass to the call to
            :py:meth:`docker.client.containers.run`.
        """
        super(DockerContainerManager, self).__init__(clipper_public_hostname)
        if "DOCKER_API_VERSION" in os.environ:
            self.docker_client = docker.from_env(
                version=os.environ["DOCKER_API_VERSION"])
        else:
            self.docker_client = docker.from_env()
        self.redis_port = redis_port
        self.redis_ip = redis_ip
        self.extra_container_kwargs = extra_container_kwargs
        self.public_hostname = clipper_public_hostname
        self.query_frontend_name = "query_frontend"
        self.mgmt_frontend_name = "mgmt_frontend"
        if registry is not None:
            # TODO: test with provided registry
            self.registry = registry
            if registry_username is not None and registry_password is not None:
                logger.info("Logging in to {registry} as {user}".format(
                    registry=registry, user=registry_username))
                login_response = self.docker_client.login(
                    username=registry_username,
                    password=registry_password,
                    registry=registry)
                logger.info(login_response)

        # TODO: Deal with Redis persistence

    def start_clipper(self):
        try:
            self.docker_client.networks.create(DOCKER_NETWORK_NAME, check_duplicate=True)
        except docker.errors.APIError as e:
            logger.info("{nw} network already exists".format(nw=DOCKER_NETWORK_NAME))
        container_args = {
            "network": DOCKER_NETWORK_NAME,
            "labels": {
                CLIPPER_DOCKER_LABEL: ""
            },
            "detach": True,
        }
        self.extra_container_kwargs.update(container_args)

        if self.redis_ip is None:
            logger.info("Starting managed Redis instance in Docker")
            self.redis_ip = "redis"
            self.docker_client.containers.run(
                'redis:alpine',
                "redis-server --port %d" % self.redis_port,
                name="redis",
                ports={'%s/tcp' % self.redis_port: self.redis_port},
                **self.extra_container_kwargs)

        cmd = "--redis_ip={redis_ip} --redis_port={redis_port}".format(
            redis_ip=self.redis_ip, redis_port=self.redis_port)
        self.docker_client.containers.run(
            'clipper/management_frontend:latest',
            cmd,
            name=self.mgmt_frontend_name,
            ports={'%s/tcp' % CLIPPER_MANAGEMENT_PORT: CLIPPER_MANAGEMENT_PORT},
            **self.extra_container_kwargs)
        self.docker_client.containers.run(
            'clipper/query_frontend:latest',
            cmd,
            name=self.query_frontend_name,
            ports={
                '%s/tcp' % CLIPPER_QUERY_PORT: CLIPPER_QUERY_PORT,
                '%s/tcp' % CLIPPER_RPC_PORT: CLIPPER_RPC_PORT
            },
            **self.extra_container_kwargs)

    def deploy_model(self, name, version, input_type, repo, num_replicas=1):
        """
        Parameters
        ----------
        repo : str
            The fully specified Docker repository to deploy. If using a custom
            registry, the registry name must be prepended to the repo. For example,
            "localhost:5000/my_model_name:my_model_version" or
            "quay.io/my_namespace/my_model_name:my_model_version"
        """
        for _ in range(num_replicas):
            self.add_replica(name, version, input_type, repo)

    def add_replica(self, name, version, input_type, repo):
        """
        Parameters
        ----------
        repo : str
            The fully specified Docker repository to deploy. If using a custom
            registry, the registry name must be prepended to the repo. For example,
            "localhost:5000/my_model_name:my_model_version" or
            "quay.io/my_namespace/my_model_name:my_model_version"
        """

        env_vars = {
            "CLIPPER_MODEL_NAME": name,
            "CLIPPER_MODEL_VERSION": version,
            # NOTE: assumes this container being launched on same machine
            # in same docker network as the query frontend
            "CLIPPER_IP": self.query_frontend_name,
            "CLIPPER_INPUT_TYPE": input_type,
        }
        self.extra_container_kwargs["labels"][
            CLIPPER_MODEL_CONTAINER_LABEL] = "{name}:{version}".format(
                name=name, version=version)
        self.docker_client.containers.run(
            repo, environment=env_vars, **self.extra_container_kwargs)

    def get_logs(self, logging_dir):
        containers = self.docker_client.containers.list(
            filters={"label": CLIPPER_DOCKER_LABEL})
        logging_dir = os.path.abspath(os.path.expanduser(logging_dir))

        log_files = []
        if not os.path.exists(logging_dir):
            os.makedirs(logging_dir)
            logger.info("Created logging directory: %s" % logging_dir)
        for c in containers:
            log_file_name = "image_{image}:container_{id}.log".format(
                image=c.image.short_id, id=c.short_id)
            log_file = os.path.join(logging_dir, log_file_name)
            with open(log_file, "w") as lf:
                lf.write(c.logs(stdout=True, stderr=True))
            log_files.append(log_file)
        return log_files

    def stop_models(self, model_name=None, keep_version=None):
        containers = self.docker_client.containers.list(
            filters={"label": CLIPPER_MODEL_CONTAINER_LABEL})
        for c in containers:
            c_name, c_version = c.labels[CLIPPER_MODEL_CONTAINER_LABEL].split(
                ":")
            if model_name is not None and model_name == c_name:
                if keep_version is None or keep_version != c_version:
                    c.stop()

    def stop_clipper(self):
        containers = self.docker_client.containers.list(
            filters={"label": CLIPPER_DOCKER_LABEL})
        for c in containers:
            c.stop()
