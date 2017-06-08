from abc import ABCMeta, abstractmethod

class ContainerManager(metaclass=ABCMeta):

    """A manager for running containers.

    Used to launch containers as well as track currently running :class:`container.Container` instances. """

    @abstractmethod
    def start(self):
        pass

    @abstractmethod
    def get_containers(self):
        """Returns all the containers managed by this :class:`container_manager.ContainerManager` instance."""
        pass
