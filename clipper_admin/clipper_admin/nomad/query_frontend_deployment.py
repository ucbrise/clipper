from .utils import nomad_job_prefix, query_frontend_job_prefix, query_frontend_service_check, query_frontend_rpc_check
import os


""" Nomad payload to deploy a new query frontend"""
def query_frontend_deployment(job_id, datacenters, cluster_name, image, redis_ip, redis_port, num_replicas, cache_size, thread_pool_size, timeout_request, timeout_content):
    job = {
            'Job': {
                'ID': job_id,
                'Datacenters': datacenters,
                'Type': 'service',
                'TaskGroups': [
                    {
                        'Name': nomad_job_prefix(cluster_name),
                        'Count': num_replicas,
                        'Tasks': [
                            {
                                'Name': query_frontend_job_prefix(cluster_name),
                                'Driver': 'docker',
                                'Config': {
                                    'args': [
                                        "--redis_ip={}".format(redis_ip or os.environ('REDIS_SERVICE_IP')), # If redis_service_host == None, default to env var
                                        "--redis_port={}".format(redis_port or os.environ('REDIS_SERVICE_PORT') or True),
                                        "--prediction_cache_size={}".format(cache_size),
                                        "--thread_pool_size={}".format(thread_pool_size),
                                        "--timeout_request={}".format(timeout_request),
                                        "--timeout_content={}".format(timeout_content)
                                        ],
                                    'image': image,
                                    'port_map': [
                                        {'rpc': 7000},
                                        {'service': 1337}
                                        ]
                                    },
                                'Resources': {
                                    'CPU': 500,
                                    'MemoryMB': 256,
                                    'Networks': [
                                        {
                                            'DynamicPorts': [
                                                {'Label': 'rpc', 'Value': 7000},
                                                {'Label': 'service', 'Value': 1337},
                                                ],
                                            }
                                        ]
                                    },
                                'Services': [
                                    {
                                        'name': query_frontend_service_check(cluster_name),
                                        'tags': ['machine-learning', 'clipper', 'query-frontend', 'urlprefix-/clipper strip=/clipper'],
                                        'portlabel': 'service',
                                        'checks': [
                                            {
                                                'name': 'alive',
                                                'type': 'tcp',
                                                'interval': 3000000000,
                                                'timeout':  2000000000
                                                }
                                            ]
                                        },
                                    {
                                        'name': query_frontend_rpc_check(cluster_name),
                                        'tags': ['machine-learning', 'clipper', 'query-frontend', "urlprefix-:7000 proto=tcp"],
                                        'portlabel': 'rpc',
                                        'checks': [
                                            {
                                                'name': 'alive',
                                                'type': 'tcp',
                                                'interval': 3000000000,
                                                'timeout':  2000000000
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
