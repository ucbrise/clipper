import random
import tempfile
import os

from ..container_manager import CLIPPER_INTERNAL_FLUENTD_PORT


FLUENTD_VERSION             = 'v1.3-debian-1' # TODO needs to be update to receive env variable like prometheus
FLUENTD_CONF_PATH_IN_DOCKER = '/fluentd/etc/fluent.conf'
FLUENTD_DEFAULT_CONF_PATH   = '{current_dir}/clipper_fluentd.conf' \
                                .format(current_dir=os.path.dirname(os.path.abspath(__file__)))


def run_fluentd_image(docker_client, fluentd_labels, fluend_port, fluentd_conf_path, extra_container_kwargs):
    fluentd_cmd = [] # No cmd is required.
    fluentd_container_id = random.randint(0, 100000)
    fluentd_name = "fluentd-{}".format(fluentd_container_id)
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
        labels=fluentd_labels,
        **extra_container_kwargs)


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
        @param customized_conf_file: Decide whether or not to provide customized configuration file.
                If false, it will use clipper default conf file
        """
        self.conf = self.get_base_config()
        self._file_path = self.build_temp_file()

    def set_forward_address(self, address, port):
        """
        Set the port number and address of external fluentd instance (internal is the clipper fluentd)
        in which centralized logs will be forwarded

        @param port: port number to forward logs
        """
        raise NotImplementedError("set_forward_address is not implemented yet. It will be coming soon.")

    def set_directory(self, dir):
        raise NotImplementedError("set_directory is not implemented yet. It will be coming soon.")

    def set_file_name(self, name):
        raise NotImplementedError("set_file_name is not implemented yet. It will be coming soon.")

    def provide_customized_file(self, file_path):
        """Provide a customized fluentd conf file."""
        raise NotImplementedError("provide_customized_file is not implemented yet. It will be coming soon.")

    def build(self):
        """
        Build a fluentd configuration file and return the path of it.
        fluentd_default_conf_path will be stored in clipper_admin/docker folder
        and used to write the initial conf file.

        Build should be called only once to build an initial conf file.
        Developers can customize conf file written in the self.file_path using defined interfaces.

        TODO: Interfaces for modifying fluentd config file.
        TODO: Interface for providing customized fluentd file
                instead of building a file using default fluentd conf file.

        @return: Path of fluentd config file that will be sync with Docker container.
        """
        if self._file_path is None \
                or not os.path.isfile(self._file_path):
            self._file_path = self.build_temp_file()

        # Logging-TODO Currently, it copies the default conf from clipper_fluentd.conf.
        #               We need a way to customize it.
        with open(FLUENTD_DEFAULT_CONF_PATH, 'r') as default_conf_file:
            with open(self._file_path, 'w') as fleutnd_conf:
                for line in default_conf_file:
                    fleutnd_conf.write(line)

        return self._file_path

    @property
    def get_temp_file_path(self):
        return self._file_path

    @staticmethod
    def get_base_config(self):
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

