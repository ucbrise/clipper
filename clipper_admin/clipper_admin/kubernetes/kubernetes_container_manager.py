from __future__ import absolute_import, division, print_function
from ..container_manager import (create_model_container_label,
                                 ContainerManager, CLIPPER_DOCKER_LABEL,
                                 CLIPPER_MODEL_CONTAINER_LABEL)
from ..exceptions import ClipperException

from contextlib import contextmanager
from kubernetes import client, config
from kubernetes.client.rest import ApiException
from kubernetes.client import configuration
import logging
import json
import yaml
import os
import time
from multiprocessing import Process, Manager, Value
import requests
import gevent
import traceback



logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))


@contextmanager
def _pass_conflicts():
    try:
        yield
    except ApiException as e:
        body = json.loads(e.body)
        if body['reason'] == 'AlreadyExists':
            logger.info("{} already exists, skipping!".format(body['details']))
            pass
        else:
            raise e

class RoundRobinBalancer():
    def __init__(self, num_assignees):
        self.counter = 0
        self.num = num_assignees
    def assign(self):
        old_counter = self.counter
        self.counter += 1
        self.counter %= self.num
        return old_counter
    def set_assignee_count(self, count, override = False):
        if (self.num != count or override):
            self.num = count
            self.counter = 0

class KubernetesContainerManager(ContainerManager):

    # model_lock = Value('i', 1)

    def __init__(self,
                 kubernetes_api_ip,
                 redis_ip=None,
                 redis_port=6379,
                 useInternalIP=False):
        """
        Parameters
        ----------
        kubernetes_api_ip : str
            The hostname or IP address of the Kubernetes API server for your Kubernetes cluster.
        redis_ip : str, optional
            The address of a running Redis cluster. If set to None, Clipper will start
            a Redis deployment for you.
        redis_port : int, optional
            The Redis port. If ``redis_ip`` is set to None, Clipper will start Redis on this port.
            If ``redis_ip`` is provided, Clipper will connect to Redis on this port.
        useInternalIP : bool, optional
            Use Internal IP of the K8S nodes . If ``useInternalIP`` is set to False, Clipper will throw an exception, if none of the nodes have ExternalDNS .
            If ``useInternalIP`` is set to true, Clipper will use the Internal IP of the K8S node if no ExternalDNS exists for any of the nodes.

        Note
        ----
        Clipper stores all persistent configuration state (such as registered application and model
        information) in Redis. If you want Clipper to be durable and able to recover from failures,
        we recommend configuring your own persistent and replicated Redis cluster rather than letting
        Clipper launch one for you.
        """

        self.kubernetes_api_ip = kubernetes_api_ip
        self.redis_ip = redis_ip
        self.redis_port = redis_port
        self.useInternalIP = useInternalIP

        config.load_kube_config()
        configuration.assert_hostname = False
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()


        # self.state_is_Unchanged = True
        # manager = Manager()
        # self.query_to_model = manager.dict()
        self.balancer = RoundRobinBalancer(0)



    def start_clipper(self, query_frontend_image, mgmt_frontend_image,
                      cache_size, num_replicas = 1):

        # polling = Process(target = self.poll_for_query_frontend_Change)

        if self.redis_ip is None:
            name = 'redis'
            with _pass_conflicts():
                self._k8s_beta.create_namespaced_deployment(
                    body=yaml.load(
                        open(
                            os.path.join(cur_dir,
                                         '{}-deployment.yaml'.format(name)))),
                    namespace='default')
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                body["spec"]["ports"][0]["port"] = self.redis_port
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace='default')
            time.sleep(10)
        for name, img in zip(['mgmt-frontend', 'query-frontend'],
                             [mgmt_frontend_image, query_frontend_image]):
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir,
                                     '{}-deployment.yaml'.format(name))))
                if self.redis_ip is not None:
                    args = [
                        "--redis_ip={}".format(self.redis_ip),
                        "--redis_port={}".format(self.redis_port)
                    ]
                    if name is 'query-frontend':
                        args.append(
                            "--prediction_cache_size={}".format(cache_size))
                    body["spec"]["template"]["spec"]["containers"][0][
                        "args"] = args
                body["spec"]["template"]["spec"]["containers"][0][
                    "image"] = img

                self._k8s_beta.create_namespaced_deployment(
                    body=body, namespace='default')
            with _pass_conflicts():
                body = yaml.load(
                    open(
                        os.path.join(cur_dir, '{}-service.yaml'.format(name))))
                self._k8s_v1.create_namespaced_service(
                    body=body, namespace='default')
        self.connect()

        deployment_name = "query-frontend"

        self._k8s_beta.patch_namespaced_deployment_scale(
            name=deployment_name,
            namespace='default',
            body={
                'spec': {
                    'replicas': num_replicas,
                }
            })



         # polling.start()

    # def reassign_models_to_new_containers(self, lost_ip, new_ip):
    #
    #     logger.info("Query frontend has crashed: Old ip is {} and new ip is {}".format(lost_ip, new_ip))
    #
    #     models = self.query_to_model[lost_ip]
    #
    #     logger.info("We are now going to redeploy {} models".format(len(models)))
    #
    #     for x in range(len(models)):
    #         m = models[x]
    #
    #         self.deploy_model(m[1], __version__, "ints", m[0], 1, new_ip)
    #
    # def poll_for_query_frontend_Change(self):
    #
    #     time.sleep(10)
    #
    #     endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
    #     current_state = [addr.ip for addr in endpoints.subsets[0].addresses]
    #     query_frontend_ips = current_state
    #
    #     logger.info("Original state is {}".format(current_state))
    #
    #     while set(query_frontend_ips) == set(current_state):
    #         time.sleep(5)
    #         endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
    #         query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]
    #
    #
    #     time.sleep(10)
    #     endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
    #     query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]
    #     logger.info("Current State is {}".format(query_frontend_ips))
    #     self.reassign_models_to_new_containers(list(set(current_state) - set(query_frontend_ips))[0],
    #                                            list(set(query_frontend_ips) - set(current_state))[0])
    #
    #     self.poll_for_query_frontend_Change()



    def connect(self):
        nodes = self._k8s_v1.list_node()
        external_node_hosts = []
        for node in nodes.items:
            for addr in node.status.addresses:
                if addr.type == "ExternalDNS":
                    external_node_hosts.append(addr.address)
        if len(external_node_hosts) == 0 and (self.useInternalIP):
            msg = "No external node addresses found.Using Internal IP address"
            logger.warn(msg)
            for addr in node.status.addresses:
                if addr.type == "InternalIP":
                    external_node_hosts.append(addr.address)
        if len(external_node_hosts) == 0:
            msg = "Error connecting to Kubernetes cluster. No external node addresses found"
            logger.error(msg)
            raise ClipperException(msg)
        self.external_node_hosts = external_node_hosts
        logger.info("Found {num_nodes} nodes: {nodes}".format(
            num_nodes=len(external_node_hosts),
            nodes=", ".join(external_node_hosts)))
        try:
            mgmt_frontend_ports = self._k8s_v1.read_namespaced_service(
                name="mgmt-frontend", namespace='default').spec.ports
            for p in mgmt_frontend_ports:
                if p.name == "1338":
                    self.clipper_management_port = p.node_port
                    logger.info("Setting Clipper mgmt port to {}".format(
                        self.clipper_management_port))
            query_frontend_ports = self._k8s_v1.read_namespaced_service(
                name="query-frontend", namespace='default').spec.ports
            for p in query_frontend_ports:
                if p.name == "1337":
                    self.clipper_query_port = p.node_port
                    logger.info("Setting Clipper query port to {}".format(
                        self.clipper_query_port))
                elif p.name == "7000":
                    self.clipper_rpc_port = p.node_port
        except ApiException as e:
            logging.warn(
                "Exception connecting to Clipper Kubernetes cluster: {}".
                format(e))
            raise ClipperException(
                "Could not connect to Clipper Kubernetes cluster. "
                "Reason: {}".format(e))

    def upload_model_data(self, model_data_list):
        model_mapping = {}
        for model_data in model_data_list:
            model_mapping['query_frontend'] = model_data['query_frontend']
            model_mapping['model_info'] = {"image": model_data['image'],
                                           "name": model_data['name'],
                                           "version": model_data['version'],
                                           "input_type": model_data['input_type']
                                           }
            url = "http://{}/get_query_to_model_mapping".format('127.0.0.1:5000')
            # url = "http://{}/get_query_to_model_mapping".format(self.get_admin_addr())

            r = requests.post(url, json=model_mapping)

        logger.info("Posted successfully")

    def deploy_model(self, name, version, input_type, image, num_replicas=1, new_ip=None):
        with _pass_conflicts():

            # with KubernetesContainerManager.model_lock.get_lock():
            #     KubernetesContainerManager.model_lock.value = 1


            endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
            query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]


            self.balancer.set_assignee_count(len(query_frontend_ips), override = num_replicas > 1)

            original_name = name
            name = get_model_deployment_name(name, version)

            ret = []

            for x in range(num_replicas):
                if not new_ip:
                    deployment_name = "{}-{}-{}".format(original_name, x, version)
                    clipper_ip = query_frontend_ips[self.balancer.assign()]
                else:
                    clipper_ip = new_ip
                    try:
                        self.stop_models([original_name])
                    except Exception as e:
                        traceback.print_exc()
                    deployment_name = "{}{}".format(original_name, '-r')

                body = {
                    'apiVersion': 'extensions/v1beta1',
                    'kind': 'Deployment',
                    'metadata': {
                        "name": deployment_name
                    },
                    'spec': {
                        'replicas': 1,
                        'template': {
                            'metadata': {
                                'labels': {
                                    CLIPPER_MODEL_CONTAINER_LABEL:
                                    create_model_container_label(name, version),
                                    CLIPPER_DOCKER_LABEL:
                                    ""
                                }
                            },
                            'spec': {
                                'containers': [{
                                    'name':
                                    deployment_name,
                                    'image':
                                    image,
                                    'ports': [{
                                        'containerPort': 80
                                    }],
                                    'env': [{
                                        'name': 'CLIPPER_MODEL_NAME',
                                        'value': name
                                    }, {
                                        'name': 'CLIPPER_MODEL_VERSION',
                                        'value': str(version)
                                    }, {
                                        'name': 'CLIPPER_IP',
                                        # 'value': 'query-frontend'
                                        'value': clipper_ip
                                    }, {
                                        'name': 'CLIPPER_INPUT_TYPE',
                                        'value': input_type
                                    }]
                                }]
                            }
                        }
                    }
                }


                ret_dict = {"image": image,
                            "name": deployment_name,
                            "version": version,
                            "input_type": input_type,
                            "query_frontend": clipper_ip,
                            "admin_addr": self.get_admin_addr()}

                ret.append(ret_dict)

                self._k8s_beta.create_namespaced_deployment(
                    body=body, namespace='default')

            logger.info("Finished deploying model")

            # with KubernetesContainerManager.model_lock.get_lock():
            #     KubernetesContainerManager.model_lock.value = 0

        self.upload_model_data(ret)

    def get_num_replicas(self, name, version):
        deployment_name = get_model_deployment_name(name, version)
        response = self._k8s_beta.read_namespaced_deployment_scale(
            name=deployment_name, namespace='default')

        return response.spec.replicas

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        # NOTE: assumes `metadata.name` can identify the model deployment.
        deployment_name = get_model_deployment_name(name, version)

        self._k8s_beta.patch_namespaced_deployment_scale(
            name=deployment_name,
            namespace='default',
            body={
                'spec': {
                    'replicas': num_replicas,
                }
            })

    def get_logs(self, logging_dir):
        logging_dir = os.path.abspath(os.path.expanduser(logging_dir))

        log_files = []
        if not os.path.exists(logging_dir):
            os.makedirs(logging_dir)
            logger.info("Created logging directory: %s" % logging_dir)

        for pod in self._k8s_v1.list_namespaced_pod(
                namespace='default',
                label_selector=CLIPPER_DOCKER_LABEL).items:
            for i, c in enumerate(pod.status.container_statuses):
                # log_file_name = "image_{image}:container_{id}.log".format(
                #     image=c.image_id, id=c.container_id)
                log_file_name = "{pod}_{num}.log".format(
                    pod=pod.metadata.name, num=str(i))
                log_file_alt = "{cname}.log".format(cname=c.name)
                logger.info("log file name: {}".format(log_file_name))
                logger.info("log alt file name: {}".format(log_file_alt))
                log_file = os.path.join(logging_dir, log_file_name)
                with open(log_file, "w") as lf:
                    lf.write(
                        self._k8s_v1.read_namespaced_pod_log(
                            namespace='default',
                            name=pod.metadata.name,
                            container=c.name))
                log_files.append(log_file)
        return log_files

    def stop_models(self, models):
        # Stops all deployments of pods running Clipper models with the specified
        # names and versions.
        try:
            for m in models:
                for v in models[m]:
                    self._k8s_beta.delete_collection_namespaced_deployment(
                        namespace='default',
                        label_selector="{label}:{val}".format(
                            label=CLIPPER_MODEL_CONTAINER_LABEL,
                            val=create_model_container_label(m, v)))
        except ApiException as e:
            logger.warn(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all_model_containers(self):
        try:
            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default',
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logger.warn(
                "Exception deleting kubernetes deployments: {}".format(e))
            raise e

    def stop_all(self):
        logger.info("Stopping all running Clipper resources")

        try:
            for service in self._k8s_v1.list_namespaced_service(
                    namespace='default',
                    label_selector=CLIPPER_DOCKER_LABEL).items:
                service_name = service.metadata.name
                self._k8s_v1.delete_namespaced_service(
                    namespace='default', name=service_name)

            self._k8s_beta.delete_collection_namespaced_deployment(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_beta.delete_collection_namespaced_replica_set(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_replication_controller(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default', label_selector=CLIPPER_DOCKER_LABEL)

            self._k8s_v1.delete_collection_namespaced_pod(
                namespace='default',
                label_selector=CLIPPER_MODEL_CONTAINER_LABEL)
        except ApiException as e:
            logging.warn(
                "Exception deleting kubernetes resources: {}".format(e))

    def get_registry(self):
        return self.registry

    def get_admin_addr(self):
        return "{host}:{port}".format(
            host=self.external_node_hosts[0],
            port=self.clipper_management_port)

    def get_query_addr(self):
        return "{host}:{port}".format(
            host=self.external_node_hosts[0], port=self.clipper_query_port)


def get_model_deployment_name(name, version):
    return "{name}-{version}-deployment".format(name=name, version=version)
