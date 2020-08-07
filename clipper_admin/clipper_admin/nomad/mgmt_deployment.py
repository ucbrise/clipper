from .utils import nomad_job_prefix, mgmt_job_prefix, mgmt_check
import os


""" Nomad payload to deploy a new mgmt """
def mgmt_deployment(
    job_id, 
    datacenters, 
    cluster_name, 
    image, 
    redis_ip, 
    redis_port, 
    num_replicas,
    cpu=500, 
    memory=256,
    health_check_interval=3000000000,
    health_check_timeout=2000000000
    ):
    job = { 
            'Job': 
            {
                'ID': job_id,
                'Datacenters': datacenters,
                'Type': 'service',
                'TaskGroups': [
                    {
                        'Name': nomad_job_prefix(cluster_name),
                        'Count': num_replicas,
                        'Tasks': [
                            {
                                'Name': mgmt_job_prefix(cluster_name),
                                'Driver': 'docker',
                                'Config': {
                                    'args': [
                                        "--redis_ip={}".format(redis_ip or os.environ('REDIS_SERVICE_IP')), # If redis_service_host == None, default to env var
                                        "--redis_port={}".format(redis_port or os.environ('REDIS_SERVICE_PORT') or True)
                                        ],
                                    'image': image,
                                    'port_map': [
                                        {'http': 1338}     
                                        ]
                                    },
                                'Resources': {
                                    'CPU': cpu,
                                    'MemoryMB': memory,
                                    'Networks': [
                                        {
                                            'DynamicPorts': [{'Label': 'http', 'Value': 1338}]
                                            }
                                        ]
                                    },
                                'Services': [
                                    {
                                        'Name': mgmt_check(cluster_name),
                                        'Tags': ['machine-learning', 'model', 'clipper', 'mgmt'],
                                        'PortLabel': 'http',
                                        'Checks': [
                                            {
                                                'Name': 'alive',
                                                'Type': 'tcp',
                                                'interval': health_check_interval,
                                                'timeout': health_check_timeout 
                                                }
                                            ]
                                        }
                                    ]
                                }
                            ]
                        }
                    ]

                }
    }
    return job
