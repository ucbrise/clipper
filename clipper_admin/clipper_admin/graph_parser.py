

def funcname(self, parameter_list):
    pass


def get_all_nodes(dag_description):
    
    list = dag_description.split('\n')

    nodes_number = int(list[0])

    return list[2: 2+nodes_number]


def get_name_version(model_name):

    list =model_name.split('-')
    return list[0],list[1],list[2]

def expand_dag(dag_description, container_info, proxy_info):

    new_list = []

    list = dag_description.split('\n')

    nodes_number = int(list[0])

    new_list.append(list[0])
    new_list.append(list[1])

    for node_info in list[2: 2+nodes_number]:
        
        new_info = 
