from __future__ import absolute_import, division, print_function

import socket

import random
import docker
import docker.errors
import logging
import os
import tempfile
from ..container_manager import (
    create_model_container_label, parse_model_container_label,
    ContainerManager, CLIPPER_DOCKER_LABEL, CLIPPER_MODEL_CONTAINER_LABEL,
    CLIPPER_QUERY_FRONTEND_CONTAINER_LABEL,
    CLIPPER_MGMT_FRONTEND_CONTAINER_LABEL, CLIPPER_INTERNAL_RPC_PORT,
    CLIPPER_INTERNAL_MANAGEMENT_PORT, CLIPPER_INTERNAL_QUERY_PORT,
    CLIPPER_INTERNAL_METRIC_PORT, CLIPPER_INTERNAL_REDIS_PORT,
    CLIPPER_DOCKER_PORT_LABELS, CLIPPER_METRIC_CONFIG_LABEL, ClusterAdapter,
    CLIPPER_FLUENTD_CONFIG_LABEL, CLIPPER_INTERNAL_FLUENTD_PORT)
from ..exceptions import ClipperException
from .docker_metric_utils import (
    run_query_frontend_metric_image,
    setup_metric_config,
    run_metric_image,
    add_to_metric_config,
    delete_from_metric_config
)
from .logging.docker_logging_utils import (
    get_logs_from_containers,
    get_default_log_config
)
from .docker_api_utils import (
    create_network,
    check_container_status,
    list_containers,
    run_container
)
from clipper_admin.docker.logging.fluentd import Fluentd


logger = logging.getLogger(__name__)


class DockerContainerManager(ContainerManager):
    def __init__(self,
                 cluster_name="default-cluster",
                 docker_ip_address="localhost",
                 use_centralized_log=False,
                 fluentd_port=CLIPPER_INTERNAL_FLUENTD_PORT,
                 clipper_query_port=1337,
                 clipper_management_port=1338,
                 clipper_rpc_port=7000,
                 redis_ip=None,
                 redis_port=6379,
                 prometheus_port=9090,
                 docker_network="clipper_network",
                 extra_container_kwargs=None):
        """
        Parameters
        ----------
        cluster_name : str
            A unique name for this Clipper cluster. This can be used to run multiple Clipper
            clusters on the same node without interfering with each other.
        docker_ip_address : str, optional
            The public hostname or IP address at which the Clipper Docker
            containers can be accessed via their exposed ports. This should almost always
            be "localhost". Only change if you know what you're doing!
        use_centralized_log: bool, optional
            If it is True, Clipper sets up Fluentd and DB (Currently SQlite) to centralize logs
        fluentd_port : int, optional
            The port on which the fluentd logging driver should listen to centralize logs.
        clipper_query_port : int, optional
            The port on which the query frontend should listen for incoming prediction requests.
        clipper_management_port : int, optional
            The port on which the management frontend should expose the management REST API.
        clipper_rpc_port : int, optional
            The port to start the Clipper RPC service on.
        redis_ip : str, optional
            The address of a running Redis cluster. If set to None, Clipper will start
            a Redis container for you.
        redis_port : int, optional
            The Redis port. If ``redis_ip`` is set to None, Clipper will start Redis on this port.
            If ``redis_ip`` is provided, Clipper will connect to Redis on this port.
        docker_network : str, optional
            The docker network to attach the containers to. You can read more about Docker
            networking in the
            `Docker User Guide <https://docs.docker.com/engine/userguide/networking/>`_.
        extra_container_kwargs : dict
            Any additional keyword arguments to pass to the call to
            :py:meth:`docker.client.containers.run`.
        """
        self.cluster_name = cluster_name
        self.cluster_identifier = cluster_name  # For logging purpose
        self.public_hostname = docker_ip_address
        self.clipper_query_port = clipper_query_port
        self.clipper_management_port = clipper_management_port
        self.clipper_rpc_port = clipper_rpc_port
        self.redis_ip = redis_ip
        if redis_ip is None:
            self.external_redis = False
        else:
            self.external_redis = True
        self.redis_port = redis_port
        self.prometheus_port = prometheus_port
        self.centralize_log = use_centralized_log
        if docker_network is "host":
            raise ClipperException(
                "DockerContainerManager does not support running Clipper on the "
                "\"host\" docker network. Please pick a different network name"
            )
        self.docker_network = docker_network

        self.docker_client = docker.from_env()
        if extra_container_kwargs is None:
            self.extra_container_kwargs = {}
        else:
            self.extra_container_kwargs = extra_container_kwargs.copy()

        # Merge Clipper-specific labels with any user-provided labels
        if "labels" in self.extra_container_kwargs:
            self.common_labels = self.extra_container_kwargs.pop("labels")
            self.common_labels.update({
                CLIPPER_DOCKER_LABEL: self.cluster_name
            })
        else:
            self.common_labels = {CLIPPER_DOCKER_LABEL: self.cluster_name}

        container_args = {
            "network": self.docker_network,
            "detach": True,
        }

        self.extra_container_kwargs.update(container_args)

        self.logger = ClusterAdapter(logger, {
            'cluster_name': self.cluster_identifier
        })

        # Setting Docker cluster logging.
        self.logging_system = Fluentd
        self.log_config = get_default_log_config()
        self.logging_system_instance = None

        if self.centralize_log:
            self.logging_system_instance = self.logging_system(
                self.logger,
                self.cluster_name,
                self.docker_client,
                port=find_unbound_port(fluentd_port)
            )
            self.log_config = self.logging_system_instance.get_log_config()

    def start_clipper(self,
                      query_frontend_image,
                      mgmt_frontend_image,
                      frontend_exporter_image,
                      cache_size,
                      qf_http_thread_pool_size,
                      qf_http_timeout_request,
                      qf_http_timeout_content,
                      num_frontend_replicas=1):
        if num_frontend_replicas != 1:
            msg = "Docker container manager's query frontend scale-out " \
                  "hasn't been implemented. You can contribute to Clipper at " \
                  "https://github.com/ucbrise/clipper." \
                  "Please set num_frontend_replicas=1 or use Kubernetes."
            raise ClipperException(msg)

        create_network(
            docker_client=self.docker_client,
            name=self.docker_network)

        containers_in_cluster = list_containers(
            docker_client=self.docker_client,
            filters={
                'label': [
                    '{key}={val}'.format(
                        key=CLIPPER_DOCKER_LABEL, val=self.cluster_name)
                ]
            })
        if len(containers_in_cluster) > 0:
            raise ClipperException(
                "Cluster {} cannot be started because it already exists. "
                "Please use ClipperConnection.connect() to connect to it.".
                format(self.cluster_name))

        if self.centralize_log:
            self.logging_system_instance.start(self.common_labels, self.extra_container_kwargs)

        # Redis for cluster configuration
        if not self.external_redis:
            self.logger.info("Starting managed Redis instance in Docker")
            redis_name = "redis-{}".format(random.randint(0, 100000))
            self.redis_port = find_unbound_port(self.redis_port)
            redis_labels = self.common_labels.copy()
            redis_labels[CLIPPER_DOCKER_PORT_LABELS['redis']] = str(
                self.redis_port)
            redis_container = run_container(
                docker_client=self.docker_client,
                image='redis:alpine',
                cmd="redis-server --port %s" % CLIPPER_INTERNAL_REDIS_PORT,
                log_config=self.log_config,
                name=redis_name,
                ports={
                    '%s/tcp' % CLIPPER_INTERNAL_REDIS_PORT: self.redis_port
                },
                labels=redis_labels,
                extra_container_kwargs=self.extra_container_kwargs)
            self.redis_ip = redis_container.name
            check_container_status(
                docker_client=self.docker_client,
                name=redis_name)

        # frontend management
        mgmt_cmd = "--redis_ip={redis_ip} --redis_port={redis_port}".format(
            redis_ip=self.redis_ip, redis_port=CLIPPER_INTERNAL_REDIS_PORT)
        mgmt_name = "mgmt_frontend-{}".format(random.randint(0, 100000))
        self.clipper_management_port = find_unbound_port(
            self.clipper_management_port)
        mgmt_labels = self.common_labels.copy()
        mgmt_labels[CLIPPER_MGMT_FRONTEND_CONTAINER_LABEL] = ""
        mgmt_labels[CLIPPER_DOCKER_PORT_LABELS['management']] = str(
            self.clipper_management_port)
        run_container(
            docker_client=self.docker_client,
            image=mgmt_frontend_image,
            cmd=mgmt_cmd,
            log_config=self.log_config,
            name=mgmt_name,
            ports={
                '%s/tcp' % CLIPPER_INTERNAL_MANAGEMENT_PORT:
                    self.clipper_management_port
            },
            labels=mgmt_labels,
            extra_container_kwargs=self.extra_container_kwargs)
        check_container_status(
            docker_client=self.docker_client,
            name=mgmt_name)

        # query frontend
        query_cmd = ("--redis_ip={redis_ip} --redis_port={redis_port} "
                     "--prediction_cache_size={cache_size} "
                     "--thread_pool_size={thread_pool_size} "
                     "--timeout_request={timeout_request} "
                     "--timeout_content={timeout_content}").format(
                         redis_ip=self.redis_ip,
                         redis_port=CLIPPER_INTERNAL_REDIS_PORT,
                         cache_size=cache_size,
                         thread_pool_size=qf_http_thread_pool_size,
                         timeout_request=qf_http_timeout_request,
                         timeout_content=qf_http_timeout_content)
        query_container_id = random.randint(0, 100000)
        query_name = "query_frontend-{}".format(query_container_id)
        self.clipper_query_port = find_unbound_port(self.clipper_query_port)
        self.clipper_rpc_port = find_unbound_port(self.clipper_rpc_port)
        query_labels = self.common_labels.copy()
        query_labels[CLIPPER_QUERY_FRONTEND_CONTAINER_LABEL] = ""
        query_labels[CLIPPER_DOCKER_PORT_LABELS['query_rest']] = str(
            self.clipper_query_port)
        query_labels[CLIPPER_DOCKER_PORT_LABELS['query_rpc']] = str(
            self.clipper_rpc_port)
        run_container(
            docker_client=self.docker_client,
            image=query_frontend_image,
            cmd=query_cmd,
            log_config=self.log_config,
            name=query_name,
            ports={
                '%s/tcp' % CLIPPER_INTERNAL_QUERY_PORT: self.clipper_query_port,
                '%s/tcp' % CLIPPER_INTERNAL_RPC_PORT: self.clipper_rpc_port
            },
            labels=query_labels,
            extra_container_kwargs=self.extra_container_kwargs)
        check_container_status(
            docker_client=self.docker_client,
            name=query_name)

        # Metric Section
        query_frontend_metric_name = "query_frontend_exporter-{}".format(
            query_container_id)
        run_query_frontend_metric_image(
            query_frontend_metric_name, self.docker_client, query_name,
            frontend_exporter_image, self.common_labels,
            self.log_config, self.extra_container_kwargs)
        check_container_status(
            docker_client=self.docker_client,
            name=query_frontend_metric_name)

        self.prom_config_path = tempfile.NamedTemporaryFile(
            'w', suffix='.yml', delete=False).name
        self.prom_config_path = os.path.realpath(
            self.prom_config_path)  # resolve symlink
        self.logger.info("Metric Configuration Saved at {path}".format(
            path=self.prom_config_path))
        setup_metric_config(query_frontend_metric_name, self.prom_config_path,
                            CLIPPER_INTERNAL_METRIC_PORT)

        metric_frontend_name = "metric_frontend-{}".format(
            random.randint(0, 100000))
        self.prometheus_port = find_unbound_port(self.prometheus_port)
        metric_labels = self.common_labels.copy()
        metric_labels[CLIPPER_DOCKER_PORT_LABELS['metric']] = str(
            self.prometheus_port)
        metric_labels[CLIPPER_METRIC_CONFIG_LABEL] = self.prom_config_path
        run_metric_image(metric_frontend_name, self.docker_client,
                         metric_labels, self.prometheus_port,
                         self.prom_config_path, self.log_config,
                         self.extra_container_kwargs)
        check_container_status(
            docker_client=self.docker_client,
            name=metric_frontend_name)

        self.connect()

    def connect(self):
        """
        Use the cluster name to update ports. Because they might not match as in
        start_clipper the ports might be changed.
        :return: None
        """
        containers = list_containers(
            docker_client=self.docker_client,
            filters={
                'label': [
                    '{key}={val}'.format(
                        key=CLIPPER_DOCKER_LABEL, val=self.cluster_name)
                ]
            })
        all_labels = {}
        for container in containers:
            all_labels.update(container.labels)

        if CLIPPER_DOCKER_PORT_LABELS['redis'] in all_labels:
            self.redis_port = all_labels[CLIPPER_DOCKER_PORT_LABELS['redis']]
        self.clipper_management_port = all_labels[CLIPPER_DOCKER_PORT_LABELS[
            'management']]
        self.clipper_query_port = all_labels[CLIPPER_DOCKER_PORT_LABELS[
            'query_rest']]
        self.clipper_rpc_port = all_labels[CLIPPER_DOCKER_PORT_LABELS[
            'query_rpc']]
        self.prometheus_port = all_labels[CLIPPER_DOCKER_PORT_LABELS['metric']]
        self.prom_config_path = all_labels[CLIPPER_METRIC_CONFIG_LABEL]
        if self._is_valid_logging_state_to_connect(all_labels):
            self.connect_to_logging_system(all_labels)

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
        containers = list_containers(
            docker_client=self.docker_client,
            filters={
                "label": [
                    "{key}={val}".format(
                        key=CLIPPER_DOCKER_LABEL, val=self.cluster_name),
                    "{key}={val}".format(
                        key=CLIPPER_MODEL_CONTAINER_LABEL,
                        val=create_model_container_label(name, version))
                ]
            })
        return containers

    def get_num_replicas(self, name, version):
        return len(self._get_replicas(name, version))

    def _add_replica(self, name, version, input_type, image):

        containers = list_containers(
            docker_client=self.docker_client,
            filters={
                "label": [
                    "{key}={val}".format(
                        key=CLIPPER_DOCKER_LABEL, val=self.cluster_name),
                    CLIPPER_QUERY_FRONTEND_CONTAINER_LABEL
                ]
            })
        if len(containers) < 1:
            self.logger.warning("No Clipper query frontend found.")
            raise ClipperException(
                "No Clipper query frontend to attach model container to")
        query_frontend_hostname = containers[0].name
        env_vars = {
            "CLIPPER_MODEL_NAME": name,
            "CLIPPER_MODEL_VERSION": version,
            # NOTE: assumes this container being launched on same machine
            # in same docker network as the query frontend
            "CLIPPER_IP": query_frontend_hostname,
            "CLIPPER_INPUT_TYPE": input_type,
        }

        model_container_label = create_model_container_label(name, version)
        labels = self.common_labels.copy()
        labels[CLIPPER_MODEL_CONTAINER_LABEL] = model_container_label
        labels[CLIPPER_DOCKER_LABEL] = self.cluster_name

        model_container_name = model_container_label + '-{}'.format(
            random.randint(0, 100000))

        run_container(
            docker_client=self.docker_client,
            image=image,
            name=model_container_name,
            environment=env_vars,
            labels=labels,
            log_config=self.log_config,
            extra_container_kwargs=self.extra_container_kwargs)

        # Metric Section
        add_to_metric_config(model_container_name, self.prom_config_path,
                             self.prometheus_port,
                             CLIPPER_INTERNAL_METRIC_PORT)

        # Return model_container_name so we can check if it's up and running later
        return model_container_name

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        current_replicas = self._get_replicas(name, version)
        if len(current_replicas) < num_replicas:
            num_missing = num_replicas - len(current_replicas)
            self.logger.info(
                "Found {cur} replicas for {name}:{version}. Adding {missing}".
                format(
                    cur=len(current_replicas),
                    name=name,
                    version=version,
                    missing=(num_missing)))

            model_container_names = []
            for _ in range(num_missing):
                container_name = self._add_replica(name, version, input_type,
                                                   image)
                model_container_names.append(container_name)
            for name in model_container_names:
                check_container_status(
                    docker_client=self.docker_client,
                    name=name)

        elif len(current_replicas) > num_replicas:
            num_extra = len(current_replicas) - num_replicas
            self.logger.info(
                "Found {cur} replicas for {name}:{version}. Removing {extra}".
                format(
                    cur=len(current_replicas),
                    name=name,
                    version=version,
                    extra=(num_extra)))
            while len(current_replicas) > num_replicas:
                cur_container = current_replicas.pop()
                cur_container.stop()
                # Metric Section
                delete_from_metric_config(cur_container.name,
                                          self.prom_config_path,
                                          self.prometheus_port)

    def get_logs(self, logging_dir):
        if self.centralize_log:
            return self.logging_system_instance.get_logs(logging_dir)
        else:
            return get_logs_from_containers(self, logging_dir)

    def stop_models(self, models):
        containers = list_containers(
            docker_client=self.docker_client,
            filters={
                "label": [
                    CLIPPER_MODEL_CONTAINER_LABEL, "{key}={val}".format(
                        key=CLIPPER_DOCKER_LABEL, val=self.cluster_name)
                ]
            })
        for c in containers:
            c_name, c_version = parse_model_container_label(
                c.labels[CLIPPER_MODEL_CONTAINER_LABEL])
            if c_name in models and c_version in models[c_name]:
                c.stop()

    def stop_all_model_containers(self):
        containers = list_containers(
            docker_client=self.docker_client,
            filters={
                "label": [
                    CLIPPER_MODEL_CONTAINER_LABEL, "{key}={val}".format(
                        key=CLIPPER_DOCKER_LABEL, val=self.cluster_name)
                ]
            })
        for c in containers:
            c.stop()

    def stop_all(self, graceful=True):
        containers = list_containers(
            docker_client=self.docker_client,
            filters={
                "label":
                "{key}={val}".format(
                    key=CLIPPER_DOCKER_LABEL, val=self.cluster_name)
            })
        for c in containers:
            if graceful:
                c.stop()
            else:
                c.kill()

    def _is_valid_logging_state_to_connect(self, all_labels):
        if self.centralize_log and not self.logging_system.container_is_running(all_labels):
            raise ClipperException(
                "Invalid state detected. "
                "log centralization is {log_centralization_state}, "
                "but cannot find fluentd instance running. "
                "Please change your use_centralized_log parameter of DockerContainermanager"
                    .format(log_centralization_state=self.centralize_log)
            )

        return self.logging_system.container_is_running(all_labels)

    def connect_to_logging_system(self, all_labels):
        if not self.centralize_log:
            logger.info(
                "The current DockerContainerManager's use_centralized_log flag is False, "
                "but there is a logging system {type} instance running in a cluster."
                "It means that clipper cluster you want to connect uses log centralization."
                "We will set the flag on to avoid unexpected bugs"
                    .format(type=self.logging_system.get_type())
            )
        self.centralize_log= True
        self.logging_system_instance = \
            self.logging_system(
                self.logger,
                self.cluster_name,
                self.docker_client,
                port=all_labels[CLIPPER_DOCKER_PORT_LABELS['fluentd']],
                conf_path=all_labels[CLIPPER_FLUENTD_CONFIG_LABEL]
            )
        self.log_config = self.logging_system_instance.get_log_config()

    def get_admin_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.clipper_management_port)

    def get_query_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.clipper_query_port)

    def get_metric_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.prometheus_port)


def find_unbound_port(start=None,
                      increment=False,
                      port_range=(10000, 50000),
                      verbose=False,
                      logger=None):
    """
    Find a unbound port.

    Parameters
    ----------
    start : int
        The port number to start with. If this port is unbounded, return this port.
        If None, start will be a random port.
    increment : bool
        If True, find port by incrementing start port; else, random search.
    port_range : tuple
        The range of port for random number generation
    verbose : bool
        Verbose flag for logging
    logger: logging.Logger
    """
    while True:
        if not start:
            start = random.randint(*port_range)

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.bind(("127.0.0.1", start))
            # Make sure we clean up after binding
            del sock
            return start
        except socket.error as e:
            if verbose and logger:
                logger.info("Socket error: {}".format(e))
                logger.info(
                    "randomly generated port %d is bound. Trying again." %
                    start)

        if increment:
            start += 1
        else:
            start = random.randint(*port_range)


