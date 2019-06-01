from clipper_admin.deployers import python as python_deployer
from clipper_admin import ClipperConnection, DockerContainerManager


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    # parser.add_argument('-n', '--num_nodes', type=int, default=3)
    parser.add_argument('node_id', type=int)
    args = parser.parse_args()

    # num_nodes = args.num_nodes
    node_id = args.node_id

    clipper_conn = ClipperConnection(DockerContainerManager(
        cluster_name='clipper_cluster_{}'.format(node_id),
        docker_ip_address='localhost',
        clipper_query_port=1337+node_id,
        clipper_management_port=2337+node_id,
        clipper_rpc_port=7000+node_id,
        redis_ip=None,
        redis_port=6379+node_id,
        prometheus_port=9090+node_id,
        # WARING: DO NOT CHANGE THE RULE OF NETWORK NAMES 
        docker_network='clipper_network_{}'.format(node_id),
        # SINCE THIS IS USED BY reset.sh TO IDENTIFY CLIPPER CONTAINERS
        extra_container_kwargs={})) # for node_id in range(args.num_nodes)]

    try:
        clipper_conn.start_clipper()
        clipper_conn.register_application(name="default", input_type="string", default_output="", slo_micros=100000)

        clipper_conn.register_model(name="image-model", version="1", input_type="string", image="yourimage")
      
        clipper_conn.link_model_to_app(app_name="default", model_name="image-model")
    except:
        exit(1)

    exit(0)
