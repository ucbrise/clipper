from __future__ import absolute_import

from .docker.docker_container_manager import DockerContainerManager
from .kubernetes.kubernetes_container_manager import KubernetesContainerManager
from .clipper_admin import *
from . import deployers
from .version import __version__, __registry__
from .exceptions import ClipperException, UnconnectedException
