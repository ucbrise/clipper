import abc
from abc import abstractmethod

class LoadBalancer(abc.ABC):

    @abstractmethod
    def tcp(self, address):
        pass
    @abstractmethod
    def http(self, address):
        pass

