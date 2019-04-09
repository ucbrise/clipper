import random
import tempfile
import os

from clipper_admin.container_manager import (
    CLIPPER_INTERNAL_FLUENTD_PORT, CLIPPER_FLUENTD_CONFIG_LABEL,
    CLIPPER_DOCKER_PORT_LABELS
)


FLUENTD_VERSION = 'v1.3-debian-1'  # TODO needs to be update to receive env variable like prometheus
FLUENTD_CONF_PATH_IN_DOCKER = '/fluentd/etc/fluent.conf'
FLUENTD_DEFAULT_CONF_PATH = '{current_dir}/clipper_fluentd.conf' \
    .format(current_dir=os.path.dirname(os.path.abspath(__file__)))


class Fluentd(object):
    def __init__(self, logger, cluster_name, docker_client, port=24224, conf_path=None):
        self.port = port
        self.logger = logger
        self.docker_client = docker_client
        self.cluster_name = cluster_name
        self.conf_path = conf_path

    def start(self, clipper_common_labels, extra_container_kwargs):
        self.logger.info("Starting Fluentd instance in Docker cluster {cluster_name}"
                         .format(cluster_name=self.cluster_name))

        self.fluentd_conf_path = FluentdConfig().build(self.port)
        fluentd_labels = self._get_labels(clipper_common_labels)

        self.logger.info(
            "Fluentd Configuration Saved at {path}. "
            "It will be mounted at {mounted_path} inside container"
                .format(path=self.fluentd_conf_path, mounted_path=str(FluentdConfig.get_conf_path_within_docker())))

        self._run_fluentd_image(self.docker_client, fluentd_labels,
                                self.port, self.fluentd_conf_path, extra_container_kwargs)

    def get_logs(self, logging_dir):
        raise NotImplementedError("Not implemented yet.")

    @staticmethod
    def container_is_running(all_labels):
        return CLIPPER_DOCKER_PORT_LABELS['fluentd'] in all_labels

    @staticmethod
    def get_type():
        return "Fluentd"

    def _run_fluentd_image(self, docker_client, fluentd_labels, fluend_port, fluentd_conf_path, extra_container_kwargs):
        fluentd_cmd = []  # No cmd is required.
        fluentd_name = "fluentd-{}".format(random.randint(0, 100000))
        fluentd_img = 'fluent/fluentd:{version}'.format(version=FLUENTD_VERSION)

        docker_client.containers.run(
            fluentd_img,
            command=fluentd_cmd,
            name=fluentd_name,
            ports={
                '%s/tcp' % CLIPPER_INTERNAL_FLUENTD_PORT: fluend_port,
                '%s/udp' % CLIPPER_INTERNAL_FLUENTD_PORT: fluend_port
            },
            volumes={
                fluentd_conf_path: {
                    'bind': '{conf_path_within_docker}'.format(conf_path_within_docker=FLUENTD_CONF_PATH_IN_DOCKER),
                    'mode': 'rw'
                }
            },
            user='root',
            labels=fluentd_labels,
            **extra_container_kwargs)

    def _get_labels(self, clipper_common_labels):
        fluentd_labels = clipper_common_labels.copy()
        fluentd_labels[CLIPPER_FLUENTD_CONFIG_LABEL] = self.fluentd_conf_path
        fluentd_labels[CLIPPER_DOCKER_PORT_LABELS['fluentd']] = str(self.port)

        return fluentd_labels

    def get_log_config(self):
        return {
            'type': 'fluentd',
            'Config': {
                'fluentd-address': '127.0.0.1:{port}'.format(port=self.port)
            }
        }


class FluentdConfig:
    """
    Class to build a fluentd config file.

    EX) FluentdConfig() # will build an initial conf file with initial configuration.
            .set_forward_address(OTHER_FLUENTD_ADDRESS, OTHER_FLUENTD_PORT)
            .set_directory(DIR_NAME)
            .set_file_name(FILE_NAME)
            .build()
    """

    def __init__(self, customized_conf_file=False):
        """
        :param customized_conf_file: Decide whether or not to provide customized configuration file.
                If false, it will use clipper default conf file
        """
        self.conf = self.base_config
        self._file_path = self.build_temp_file()

    def set_forward_address(self, address, port):
        """
        Set the port number and address of external fluentd instance (internal is the clipper fluentd)
        in which centralized logs will be forwarded

        :param port: port number to forward logs
        """
        raise NotImplementedError("set_forward_address is not implemented yet. It will be coming soon.")

    def build(self, fluentd_port):
        """
        Build a fluentd configuration file and return the path of it.
        fluentd_default_conf_path will be stored in clipper_admin/docker folder
        and used to write the initial conf file.

        Build should be called only once to build an initial conf file.
        Developers can customize conf file written in the self.file_path using defined interfaces.

        TODO: Interfaces for modifying fluentd config file.
        TODO: Interfaces for providing customized fluentd file

        :param fluentd_port: External fluentd port in which fluentd with this conf file listens to
        :return: Path of fluentd config file in which Fluentd container mounts on.
        """
        if self._file_path is None \
                or not os.path.isfile(self._file_path):
            self._file_path = self.build_temp_file()

        # Logging-TODO: Currently, it copies the default conf from clipper_fluentd.conf.
        #               We need a way to customize it.
        with open(FLUENTD_DEFAULT_CONF_PATH, 'r') as default_conf_file:
            with open(self._file_path, 'w') as fluetnd_conf:
                for line in default_conf_file:
                    # port number in a conf file should be the same as container manager's port number
                    fluetnd_conf.write(line)

        return self._file_path

    @property
    def temp_file_path(self):
        return self._file_path

    @property
    def base_config(self):
        return {}

    @staticmethod
    def get_conf_path_within_docker():
        return FLUENTD_CONF_PATH_IN_DOCKER

    @staticmethod
    def build_temp_file():
        file_path = tempfile.NamedTemporaryFile(
            'w', suffix='.conf', delete=False).name
        file_path = os.path.realpath(
            file_path)  # resolve symlink
        return file_path