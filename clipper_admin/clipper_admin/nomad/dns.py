import abc
from abc import abstractmethod
class DNS(abc.ABC):

    @abstractmethod
    def resolveSRV(self, job_name):
        pass
    
