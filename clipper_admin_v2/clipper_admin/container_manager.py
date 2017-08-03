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

    @abc.abstractmethod
    def start_clipper(self,
                      query_frontend_image,
                      mgmt_frontend_image):
        return

    @abc.abstractmethod
    def connect(self):
        return

    @abc.abstractmethod
    def deploy_model(self, name, version, input_type, image):
        return

    @abc.abstractmethod
    def get_num_replicas(self, name, version):
        return

    @abc.abstractmethod
    def set_num_replicas(self, name, version, input_type, image):
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

    @abc.abstractmethod
    def get_admin_addr(self):
        return

    @abc.abstractmethod
    def get_query_addr(self):
        return
