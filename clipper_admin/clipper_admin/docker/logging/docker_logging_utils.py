import os
import sys

from clipper_admin.container_manager import CLIPPER_DOCKER_LABEL


def get_logs_from_containers(docker_container_manager, logging_dir):
    containers = docker_container_manager.docker_client.containers.list(
        filters={
            "label":
                "{key}={val}".format(
                    key=CLIPPER_DOCKER_LABEL, val=docker_container_manager.cluster_name)
        })
    logging_dir = os.path.abspath(os.path.expanduser(logging_dir))

    log_files = []
    if not os.path.exists(logging_dir):
        os.makedirs(logging_dir)
        docker_container_manager.logger.info("Created logging directory: %s" % logging_dir)
    for c in containers:
        log_file_name = "image_{image}:container_{id}.log".format(
            image=c.image.short_id, id=c.short_id)
        log_file = os.path.join(logging_dir, log_file_name)
        if sys.version_info < (3, 0):
            with open(log_file, "w") as lf:
                lf.write(c.logs(stdout=True, stderr=True))
        else:
            with open(log_file, "wb") as lf:
                lf.write(c.logs(stdout=True, stderr=True))
        log_files.append(log_file)
    return log_files


def get_default_log_config():
    # default config is defined here.
    # https://docs.docker.com/config/containers/logging/configure/
    return {'type': 'json-file'}
