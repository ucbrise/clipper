
""" Nomad payload to deploy Redis """
def redis_deployment(datacenters, cluster_name):
    job = {
        'Datacenters': datacenters,
        'Type': 'service',
        'Update': {
            'MaxParallel': 1,
            'MinHealthyTime': '10s',
            'HealthyDeadline': '3m',
            'AutoRevert': false,
            'Canary': 0
        },
        'TaskGroups': [
            {
                'Name': 'clipper-{cluster-name}'.format(cluster_name),
                'Count': 1,
                'Restart': {
                    'Attempts': 10,
                    'Interval': '5m',
                    'Delay': '25s',
                    'Mode': 'delay'
                },
                'EphemeralDisk': {
                    'Size': 300
                },
                'Tasks': [
                    'Name': 'redis-at-{cluster-name}'.format(cluster_name),
                    'Driver': 'docker',
                    'Config': {
                        'Image': 'redis:3.2',
                        'port_map': [
                            {'db': 6379}     
                        ]
                    },
                    'Resources': {
                        'CPU': 500,
                        'MemoryMB': 256
                    },
                    'Services': [
                        {
                            'Name': 'global-redis-check',
                            'Tags': ['global', 'cache'],
                            'PortLabel': 'db',
                            'Checks': [
                                {
                                    'Name': 'alive',
                                    'Type'; 'tcp',
                                    'interval': '10s',
                                    'timeout': '2s'
                                }
                            ]
                        }
                    ]
                ]
            }
        ]

    }
    return job
