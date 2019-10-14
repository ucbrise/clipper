from . import DNS
import dns.resolver

class ConsulDNS(DNS):

    def resolveSRV(job_name):
        srv_records=dns.resolver.query('{}.service.consul'.format(job_name), 'SRV')
        srvInfo = {}
        for srv in srv_records:
            srvInfo['host']     = str(srv.target).rstrip('.')
            srvInfo['port']     = srv.port
        return (srvInfo['host'], srvInfo['port'])
