from __future__ import absolute_import, division, print_function
import sys, os

cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath('{}/../clipper_admin'.format(cur_dir)))
# sys.path.append('..')

from clipper_admin.container_manager import (create_model_container_label,
                                                           ContainerManager, CLIPPER_DOCKER_LABEL,
                                                           CLIPPER_MODEL_CONTAINER_LABEL)

from clipper_admin.kubernetes.kubernetes_container_manager import KubernetesContainerManager
from clipper_admin.docker.docker_container_manager import DockerContainerManager
from clipper_admin.exceptions import ClipperException

from contextlib import contextmanager
# from clipper_admin.kubernetes import client, config
from kubernetes import client, config

import logging
import json
import yaml
import os
import time
from multiprocessing import Process, Value, RLock
from ctypes import c_bool
import redis
from flask import Flask, request
from clipper_admin.version import __version__


logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))



class Mgv2():

    app = Flask(__name__)
    pool = redis.ConnectionPool(host='localhost', port=6379)
    database_lock = Value('i', 1)
    # multi_lock = RLock()

    def __init__(self):
        config.load_kube_config()
        self._k8s_v1 = client.CoreV1Api()
        self._k8s_beta = client.ExtensionsV1beta1Api()




        self.container_manager = KubernetesContainerManager(kubernetes_api_ip="eric-dev.clipper-k8s-dev.com",
                                                            useInternalIP=True)

        # endpoints = self._k8s_v1.read_namespaced_endpoints(name="mgmt-frontend", namespace="default")
        # self.mgmt_ip = [addr.ip for addr in endpoints.subsets[0].addresses][0]



    def poll_for_query_frontend_Change(self):
        time.sleep(10)

        endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
        current_state = [addr.ip for addr in endpoints.subsets[0].addresses]
        query_frontend_ips = current_state

        logger.info("Original state is {}".format(current_state))

        while set(query_frontend_ips) == set(current_state):
            time.sleep(5)
            endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
            query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]

        time.sleep(10)
        endpoints = self._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
        query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]
        logger.info("Current State is {}".format(query_frontend_ips))
        self.reassign_models_to_new_containers(list(set(current_state) - set(query_frontend_ips))[0],
                                               list(set(query_frontend_ips) - set(current_state))[0])

        self.poll_for_query_frontend_Change()

    def reassign_models_to_new_containers(self, lost_ip, new_ip):

        while KubernetesContainerManager.model_lock.value:
            time.sleep(2)

        logger.info("Query frontend has crashed: Old ip is {} and new ip is {}".format(lost_ip, new_ip))

        with Mgv2.database_lock.get_lock():
            Mgv2.database_lock.value = 1


        connection = redis.Redis(connection_pool=Mgv2.pool)

        model_info = connection.get(lost_ip)

        model_info_lst = json.loads(model_info)

        logger.info("We are now going to redeploy {} models".format(len(model_info_lst)))

        for x in range(len(model_info_lst)):
            m = model_info_lst[x]

            self.container_manager.deploy_model(m[1], __version__, "ints", m[0], 1, new_ip)

        with Mgv2.database_lock.get_lock():
            Mgv2.database_lock.value = 0


    def dbg(self):
        last_time = time.time()

        while True:
            now = time.time() - last_time
            # print("Debug; Time is now {}", now)
            print("value of boolean is {}", Mgv2.database_lock.value)
            time.sleep(1)

    @staticmethod
    @app.route('/get_query_to_model_mapping', methods=['GET', 'POST'])
    def get_query_to_model_mapping():


        logger.info("Flask has received model info")

        try:
            query_to_model_mapping = request.get_json(force = True)


            if not isinstance(query_to_model_mapping, dict):
                query_to_model_mapping = json.loads(query_to_model_mapping)

            logger.info("Query_to_model_mapping: {}", query_to_model_mapping)

            time.sleep(5)

            key = json.dumps(query_to_model_mapping["query_frontend"])
            value = json.dumps(query_to_model_mapping["model_info"])

            while Mgv2.database_lock.value:
                time.sleep(2)

            connection = redis.Redis(connection_pool = Mgv2.pool)
            get_value = connection.get(key)

            if get_value:
                get_list = json.loads(get_value)
                get_list.append(query_to_model_mapping["model_info"])
                value = json.dumps(get_list)

            connection.set(key, value)

            logger.info("Model has been added to database")
            return "Success"
        except ValueError:
            return "Failure"






if __name__ == "__main__":
    mgv2 = Mgv2()
    #
    logger.info("Starting polling")
    polling = Process(target=mgv2.poll_for_query_frontend_Change)
    polling.start()

    # debug = Process(target = mgv2.dbg)
    # debug.start()

    # connection = redis.Redis(connection_pool=Mgv2.pool)
    # connection.set("test_key", "test_value")
    # print(connection.get("test_key"))

    logger.info("Flask App is starting")

    # logger.info("Setting flask IP to {}".format(mgv2.mgmt_ip))

    # Mgv2.app.run(host = mgv2.mgmt_ip, port=5000)
    Mgv2.app.run(host='127.0.0.1', port=5000, threaded=True)








