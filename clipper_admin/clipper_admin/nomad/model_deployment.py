from .utils import nomad_job_prefix, model_job_prefix, generate_model_job_name, model_check_name

""" Nomad payload to deploy a new model """
def model_deployment(
    job_id, 
    datacenters, 
    cluster_name, 
    model_name, 
    model_version, 
    input_type, 
    image, 
    num_replicas,
    query_frontend_ip,
    query_frontend_port,
    cpu=500, 
    memory=256,
    health_check_interval=3000000000,
    health_check_timeout=2000000000
    ):
    job = {
        'Job': {
        'ID': job_id,
        'Datacenters': datacenters,
        'Type': 'service',
        'TaskGroups': [
            {
                'Name': 'clipper-{}'.format(cluster_name),
                'Count': num_replicas,
                'Tasks': [
                    {
                    'Name': generate_model_job_name(cluster_name, model_name, model_version),
                    'Driver': 'docker',
                    'Env': {
                        'CLIPPER_MODEL_NAME': model_name,
                        'CLIPPER_MODEL_VERSION': model_version,
                        'CLIPPER_IP': query_frontend_ip,
                        'CLIPPER_PORT': query_frontend_port,
                        'CLIPPER_INPUT_TYPE': input_type
                    },
                    'Config': {
                        'image': image,
                        'port_map': [
                            {'zeromq': 1390}
                        ],
                        'dns_servers': ["${attr.unique.network.ip-address}"]
                    },
                    'Resources': {
                        'CPU': cpu,
                        'MemoryMB': memory,
                        'Networks': [
                            {
                                'DynamicPorts': [
                                    {'Label': 'zeromq', 'Value': 1390}
                                 ]
                            }
                        ]
                    },
                    'Services': [
                        {
                            'Name': model_check_name(cluster_name, model_name, model_version),
                            'Tags': ['machine-learning', 'model', 'clipper', model_name],
                            'PortLabel': 'zeromq',
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
