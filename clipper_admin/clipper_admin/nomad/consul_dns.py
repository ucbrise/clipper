from .dns import DNS
import dns.resolver
import socket

class ConsulDNS(DNS):

    def resolveSRV(self, job_name):
        addr = '{}.service.consul'.format(job_name)
        srv_records= dns.resolver.query(addr, 'SRV')
        srvInfo = {}
        for srv in srv_records:
            srvInfo['host']     = str(srv.target).rstrip('.')
            srvInfo['port']     = srv.port
        host = srvInfo['host']
        port = srvInfo['port']
        print(host)
        print(port)
        return (socket.gethostbyname(addr), port)
