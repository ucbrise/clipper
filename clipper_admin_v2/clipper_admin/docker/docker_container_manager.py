from __future__ import absolute_import, division, print_function
import docker
import logging
import os
from ..container_manager import (create_model_container_label, parse_model_container_label,
                                 ContainerManager, CLIPPER_DOCKER_LABEL,
                                 CLIPPER_MODEL_CONTAINER_LABEL, CLIPPER_INTERNAL_RPC_PORT,
                                 CLIPPER_INTERNAL_QUERY_PORT, CLIPPER_INTERNAL_MANAGEMENT_PORT)

DOCKER_NETWORK_NAME = "clipper_network"

logger = logging.getLogger(__name__)


class DockerContainerManager(ContainerManager):
    def __init__(self,
                 docker_ip_address="127.0.0.1",
                 clipper_query_port=1337,
                 clipper_management_port=1338,
                 clipper_rpc_port=7000,
                 redis_ip=None,
                 redis_port=6379,
                 extra_container_kwargs={}):
        """
        Parameters
        ----------
        docker_ip_address : str
            The public hostname or IP address at which the Clipper Docker
            containers can be accessed via their exposed ports. On macOs this
            can be set to "localhost" as Docker automatically makes exposed
            ports on Docker containers available via localhost, but on other
            operating systems this must be set explicitly.

        extra_container_kwargs : dict
            Any additional keyword arguments to pass to the call to
            :py:meth:`docker.client.containers.run`.
        """
        self.public_hostname = docker_ip_address
        self.clipper_query_port = clipper_query_port
        self.clipper_management_port = clipper_management_port
        self.clipper_rpc_port = clipper_rpc_port
        self.redis_ip = redis_ip
        self.redis_port = redis_port

        self.docker_client = docker.from_env()
        self.extra_container_kwargs = extra_container_kwargs
        self.query_frontend_name = "query_frontend"
        self.mgmt_frontend_name = "mgmt_frontend"

        # TODO: Deal with Redis persistence

    def start_clipper(self, query_frontend_image, mgmt_frontend_image):
        try:
            self.docker_client.networks.create(
                DOCKER_NETWORK_NAME, check_duplicate=True)
        except docker.errors.APIError as e:
            logger.debug(
                "{nw} network already exists".format(nw=DOCKER_NETWORK_NAME))
        container_args = {
            "network": DOCKER_NETWORK_NAME,
            "detach": True,
        }

        # Merge Clipper-specific labels with any user-provided labels
        if "labels" in self.extra_container_kwargs:
            self.extra_container_kwargs["labels"].update({CLIPPER_DOCKER_LABEL: ""})
        else:
            self.extra_container_kwargs["labels"] = {CLIPPER_DOCKER_LABEL: ""}

        self.extra_container_kwargs.update(container_args)

        if self.redis_ip is None:
            logger.info("Starting managed Redis instance in Docker")
            self.redis_ip = "redis"
            self.docker_client.containers.run(
                'redis:alpine',
                "redis-server --port %s" % self.redis_port,
                name="redis",
                ports={'%s/tcp' % self.redis_port: self.redis_port},
                **self.extra_container_kwargs)

        cmd = "--redis_ip={redis_ip} --redis_port={redis_port}".format(
            redis_ip=self.redis_ip, redis_port=self.redis_port)
        self.docker_client.containers.run(
            mgmt_frontend_image,
            cmd,
            name=self.mgmt_frontend_name,
            ports={
                '%s/tcp' % CLIPPER_INTERNAL_MANAGEMENT_PORT:
                self.clipper_management_port
            },
            **self.extra_container_kwargs)
        self.docker_client.containers.run(
            query_frontend_image,
            cmd,
            name=self.query_frontend_name,
            ports={
                '%s/tcp' % CLIPPER_INTERNAL_QUERY_PORT:
                self.clipper_query_port,
                '%s/tcp' % CLIPPER_INTERNAL_RPC_PORT: self.clipper_rpc_port
            },
            **self.extra_container_kwargs)
        self.connect()

    def connect(self):
        # No extra connection steps to take on connection
        return

    def deploy_model(self, name, version, input_type, image, num_replicas=1):
        # Parameters
        # ----------
        # image : str
        #     The fully specified Docker imagesitory to deploy. If using a custom
        #     registry, the registry name must be prepended to the image. For example,
        #     "localhost:5000/my_model_name:my_model_version" or
        #     "quay.io/my_namespace/my_model_name:my_model_version"
        self.set_num_replicas(name, version, input_type, image, num_replicas)

    def _get_replicas(self, name, version):
        containers = self.docker_client.containers.list(
            filters={
                "label":
                "{key}={val}".format(
                    key=CLIPPER_MODEL_CONTAINER_LABEL,
                    val=create_model_container_label(name, version))
            })
        return containers

    def get_num_replicas(self, name, version):
        return len(self._get_replicas(name, version))

    def _add_replica(self, name, version, input_type, image):
        """
        Parameters
        ----------
        image : str
            The fully specified Docker imagesitory to deploy. If using a custom
            registry, the registry name must be prepended to the image. For example,
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
            CLIPPER_MODEL_CONTAINER_LABEL] = create_model_container_label(name, version)
        self.docker_client.containers.run(
            image, environment=env_vars, **self.extra_container_kwargs)

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        current_replicas = self._get_replicas(name, version)
        if len(current_replicas) < num_replicas:
            num_missing = num_replicas - len(current_replicas)
            logger.info(
                "Found {cur} replicas for {name}:{version}. Adding {missing}".
                format(
                    cur=len(current_replicas),
                    name=name,
                    version=version,
                    missing=(num_missing)))
            for _ in range(num_missing):
                self._add_replica(name, version, input_type, image)
        elif len(current_replicas) > num_replicas:
            num_extra = len(current_replicas) - num_replicas
            logger.info(
                "Found {cur} replicas for {name}:{version}. Removing {extra}".
                format(
                    cur=len(current_replicas),
                    name=name,
                    version=version,
                    extra=(num_extra)))
            while len(current_replicas) > num_replicas:
                cur_container = current_replicas.pop()
                cur_container.stop()

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

    def stop_models(self, models):
        containers = self.docker_client.containers.list(
            filters={"label": CLIPPER_MODEL_CONTAINER_LABEL})
        for c in containers:
            c_name, c_version = parse_model_container_label(c.labels[CLIPPER_MODEL_CONTAINER_LABEL])
            if c_name in models and c_version in models[c_name]:
                c.stop()

    def stop_all_model_containers(self):
        containers = self.docker_client.containers.list(
            filters={"label": CLIPPER_MODEL_CONTAINER_LABEL})
        for c in containers:
            c.stop()

    def stop_all(self):
        containers = self.docker_client.containers.list(
            filters={"label": CLIPPER_DOCKER_LABEL})
        for c in containers:
            c.stop()

    def get_admin_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.clipper_management_port)

    def get_query_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.clipper_query_port)
