from .dns import DNS
import dns.resolver
import socket

"""
    Consul is a service networking solution to connect and secure services across any runtime platform and public or private cloud
"""
class ConsulDNS(DNS):

    """
        This method resolves records of IP and Ports with a SRV DNS request
        Parameters:
            domain str:
                The domain to resolve, in Consul this correspond to the healthcheck name
    """
    def resolveSRV(self, check_name):
        addr = '{}.service.consul'.format(check_name)
        srv_records= dns.resolver.query(addr, 'SRV')
        srvInfo = {}
        for srv in srv_records:
            srvInfo['host']     = str(srv.target).rstrip('.')
            srvInfo['port']     = srv.port
        host = srvInfo['host']
        port = srvInfo['port']
        return (socket.gethostbyname(host), port)
        
