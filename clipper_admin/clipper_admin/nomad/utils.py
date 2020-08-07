
def nomad_job_prefix(cluster_name):
    return 'clipper-{}'.format(cluster_name)


""" 
    redis 
"""
def redis_job_prefix(cluster_name):
    return '{}-redis'.format(nomad_job_prefix(cluster_name))
def redis_check(cluster_name):
    return '{}-db'.format(mgmt_check(cluster_name))

""" 
    query frontend
"""
def query_frontend_job_prefix(cluster_name):
    return '{}-query-frontend'.format(nomad_job_prefix(cluster_name))
def query_frontend_service_check(cluster_name):
    return '{}-service'.format(query_frontend_job_prefix(cluster_name))
def query_frontend_rpc_check(cluster_name):
    return '{}-rpc'.format(query_frontend_job_prefix(cluster_name))

""" 
    mgmt
"""
def mgmt_job_prefix(cluster_name):
    return '{}-mgmt'.format(nomad_job_prefix(cluster_name))
def mgmt_check(cluster_name):
    return '{}-http'.format(mgmt_check(cluster_name))

"""
    model
"""
def model_job_prefix(cluster_name):
    return '{}-model'.format(nomad_job_prefix(cluster_name))

def generate_model_job_name(cluster_name, model_name, model_version):
    return '{}-{}-{}'.format(model_job_prefix(cluster_name), model_name, model_version)

def model_check_name(cluster_name, model_name, model_version):
    return '{}-model-{}-{}'.format(nomad_job_prefix(cluster_name), model_name, model_version)
