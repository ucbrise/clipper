import abc
from abc import abstractmethod
from load_balancer import LoadBalancer

"""

"""
class FabioLoadBalancer(LoadBalancer):

    """
        Parameters
        ----------

        address: str
            The address at which the load balancer is located. For instance fabio.service.consul
        port: str
            The port on which the TCP proxy listens, this is not the http port on which fabio proxy http requests !
            https://fabiolb.net/feature/tcp-proxy/
    """
    def __init__(self, address, port):
        self.address = address
        self.port = port
