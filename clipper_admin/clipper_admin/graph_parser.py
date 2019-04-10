

def funcname(self, parameter_list):
    pass


def get_all_nodes(dag_description):
    
    list = dag_description.split('\n')

    nodes_number = int(list[0])

    return list[2: 2+nodes_number]


def get_name_version(model_name):
    print(model_name)
    list = model_name.split(',')
    print(list)
    return list[0],list[1],list[2]

def expand_dag(dag_description, container_info, proxy_info):

    new_list = []

    list = dag_description.split('\n')

    nodes_number = int(list[0])

    new_list.append(list[0])
    new_list.append(list[1])

    count = 0

    for node_info in list[2: 2+nodes_number]:
        
        new_info = node_info \
                    + '-' + container_info[count][0] \
                    + '-' + container_info[count][1] \
                    + '-' + container_info[count][2] \
                    + '-' + proxy_info[count][0] \
                    + '-' + proxy_info[count][1] \
                    + '-' + proxy_info[count][2]

        new_list.append(new_info)

    for edge_info in list[2+nodes_number:]:
        new_list.append(edge_info)

    expanded_dag = ""

    for info in new_list:
        expanded_dag = expanded_dag + info + "\n"

    return expanded_dag
                    
