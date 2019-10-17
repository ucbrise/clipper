import abc
from abc import abstractmethod
class DNS(abc.ABC):

    """
        This method resolves records of IP and Ports with a SRV DNS request
        Parameters:
            domain str:
                The domain to resolve
    """
    @abstractmethod
    def resolveSRV(self, domain):
        pass
    
