import abc

# Constants
CLIPPER_INTERNAL_QUERY_PORT = 1337
CLIPPER_INTERNAL_MANAGEMENT_PORT = 1338
CLIPPER_INTERNAL_RPC_PORT = 7000

CLIPPER_DOCKER_LABEL = "ai.clipper.container.label"
CLIPPER_MODEL_CONTAINER_LABEL = "ai.clipper.model_container.label"
CONTAINERLESS_MODEL_IMAGE = "NO_CONTAINER"


class ContainerManager(object):
    __metaclass__ = abc.ABCMeta

    def __init__(self,
                 clipper_public_hostname,
                 clipper_query_port=1337,
                 clipper_management_port=1338,
                 clipper_rpc_port=7000,
                 redis_ip=None,
                 redis_port=6379,
                 registry=None,
                 registry_username=None,
                 registry_password=None):
        # TODO(crankshaw): Clipper public hostname may not be known until
        # after Clipper is started.
        self.public_hostname = clipper_public_hostname
        self.clipper_query_port = clipper_query_port
        self.clipper_management_port = clipper_management_port
        self.clipper_rpc_port = clipper_rpc_port
        self.redis_ip = redis_ip
        self.redis_port = redis_port
        self.registry = None

    @abc.abstractmethod
    def start_clipper(self):
        return

    # @abc.abstractmethod
    # def start_clipper(self
    #              clipper_public_hostname,
    #              clipper_query_port=1337,
    #              clipper_management_port=1338,
    #              clipper_rpc_port=7000,
    #              redis_ip=None,
    #              redis_port=6379,
    #                   **kwargs):
    #
    #     return

    # def connect(self,
    #              clipper_public_hostname,
    #              clipper_query_port=1337,
    #              clipper_management_port=1338,
    #              clipper_rpc_port=7000,
    #              registry=None,
    #              registry_username=None,
    #              registry_password=None):
    #     self.public_hostname = clipper_public_hostname
    #     self.clipper_query_port = clipper_query_port
    #     self.clipper_management_port = clipper_management_port
    #     self.clipper_rpc_port = clipper_rpc_port

    @abc.abstractmethod
    def deploy_model(self, name, version, input_type, repo):
        return

    @abc.abstractmethod
    def get_num_replicas(self, name, version):
        return

    @abc.abstractmethod
    def set_num_replicas(self, name, version, input_type, repo):
        return

    @abc.abstractmethod
    def get_logs(self, logging_dir):
        """Get the container logs for all Docker containers launched by Clipper.

            This will get the logs for both Clipper core containers and
            any model containers deployed by Clipper admin.
            Any previous log files from existing containers will be overwritten.

        Parameters
        ----------
        logging_dir : str
            The directory to write the log files to. If the directory
            does not exist, it will be created.

        Returns
        -------
        list(str)
            The list of all log files created.
        """
        return

    @abc.abstractmethod
    def stop_models(self, model_name=None, keep_version=None):
        """Stops Docker containers serving models but leaves the core Clipper containers running.
        Parameters
        ----------
        model_name : str(optional)
            Only removes containers serving the specified the model with ``model_name``
        keep_version : str(optional)
            Leaves model containers with the specified name and version untouched. This argument
            is ignored if model_name is empty. The typical use case for this argument is to remove
            old versions of a model but keep the currently active version.
        """
        return

    @abc.abstractmethod
    def stop_clipper(self):
        pass

    def get_registry(self):
        """Return a reference to the Docker registry created by the ContainerManager
        """
        return None

    def get_admin_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.clipper_management_port)

    def get_query_addr(self):
        return "{host}:{port}".format(
            host=self.public_hostname, port=self.clipper_query_port)
