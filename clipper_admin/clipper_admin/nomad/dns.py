import abc
from abc import abstractmethod
class DNS(abc.ABC):

    @abstractmethod
    def resolveSRV(job_name):
        pass
    
