from abc import ABCMeta, abstractmethod

class Container(metaclass=ABCMeta):

    """An instance of a running Docker container, abstracted from its deployment environment.

    This could be running on the local Docker daemon, or on a Kubernetes cluster."""

    @abstractmethod
    def logs(self):
        pass

    @abstractmethod
    def inspect(self):
        pass

    @abstractmethod
    def stop(self):
        pass

    @abstractmethod
    def rm(self):
        pass

class DockerDaemonContainer(Container):
    def __init__(self):
        pass

    def logs(self):
        pass

    def inspect(self):
        pass

    def stop(self):
        pass

    def rm(self):
        pass

class K8sContainer(Container):
    def __init__(self):
        pass

    def logs(self):
        pass

    def inspect(self):
        pass

    def stop(self):
        pass

    def rm(self):
        pass
