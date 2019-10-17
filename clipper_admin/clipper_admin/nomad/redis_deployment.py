from .utils import nomad_job_prefix, redis_job_prefix, redis_check

""" Nomad payload to deploy Redis """
def redis_deployment(job_id, datacenters, cluster_name):
    job = { 'Job':  {
        'ID': job_id,
        'Datacenters': datacenters,
        'Type': 'service',
        'Update': {
            'MaxParallel': 1,
            'AutoRevert': False,
            'Canary': 0
            },
        'TaskGroups': [
            {
                'Name': nomad_job_prefix(cluster_name),
                'Count': 1,
                'EphemeralDisk': {
                    'Size': 300
                    },
                'Tasks': [
                    {
                        'Name': redis_job_prefix(cluster_name),
                        'Driver': 'docker',
                        'Config': {
                            'image': 'redis:alpine',
                            'port_map': [
                                {'db': 6379}     
                                ]
                            },
                        'Resources': {
                            'CPU': 500,
                            'MemoryMB': 256,
                            'Networks': [
                                {
                                    'DynamicPorts': [{'Label': 'db', 'Value': 6379}],
                                }
                            ]
                        },
                        'Services': [
                            {
                                'Name': redis_check(cluster_name),
                                'Tags': ['global', 'cache'],
                                'PortLabel': 'db',
                                'Checks': [
                                    {
                                        'Name': 'alive',
                                        'Type': 'tcp',
                                        'interval': 1000000000000,
                                        'timeout': 20000000000
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
