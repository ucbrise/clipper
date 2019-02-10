import random

from ..container_manager import CLIPPER_INTERNAL_FLUENTD_PORT

FLUENTD_VERSION = 'v1.3-debian-1'


def run_fluentd_image(docker_client, fluentd_labels, fluend_port, extra_container_kwargs):

    fluentd_cmd          = '' # There's no special fluentd cmd for now
    fluentd_container_id = random.randint(0, 100000)
    fluentd_name         = "fluentd-{}".format(fluentd_container_id)
    fluentd_img          = 'fluent/fluentd:{version}'.format(version=FLUENTD_VERSION)

    docker_client.containers.run(
        fluentd_img,
        fluentd_cmd,
        name=fluentd_name,
        port={
            '%s/tcp' % CLIPPER_INTERNAL_FLUENTD_PORT: fluend_port,
            '%s/udp' % CLIPPER_INTERNAL_FLUENTD_PORT: fluend_port
        },
        labels=fluentd_labels,
        detach=True,
        **extra_container_kwargs)


class FluentdConfig:
    """
    Follow a builder pattern to contruct a fluentd configuration file, fluentd.conf
    """
    def __init__(self):
        self.conf = {

        }

    def build(self):
        pass

