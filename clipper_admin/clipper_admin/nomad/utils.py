
def nomad_job_prefix(cluster_name):
    return 'clipper-{}'.format(cluster_name)

""" 
    query frontend
"""
def query_frontend_job_prefix(cluster_name):
    return '{}-query-frontend'.format(nomad_job_prefix(cluster_name))
def query_frontend_service_check(cluster_name):
    return '{}-service'.format(query_frontend_job_prefix(cluster_name))
def query_frontend_rpc_check(cluster_name):
    return '{}-rpc'.format(query_frontend_job_prefix(cluster_name))
