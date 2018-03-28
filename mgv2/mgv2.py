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
from clipper_admin import ClipperConnection

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
import gevent
from gevent import monkey
import argparse
import traceback


logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))



class Mgv2():
    monkey.patch_all()
    app = Flask(__name__)
    pool = redis.ConnectionPool(host='127.0.0.1', port=6379)
    r = redis.Redis(connection_pool = pool)
    r.flushdb()
    container_manager = None
    config.load_kube_config()
    _k8s_v1 = client.CoreV1Api()
    _k8s_beta = client.ExtensionsV1beta1Api()
    # print(r.get('wohoo'))
    # database_lock = Value('i', 1)
    # multi_lock = RLock()

    def __init__(self, redis_ip = None, redis_port = None ):


        self.red_ip = redis_ip
        self.red_port = redis_port

        if not self.red_ip:
            self.red_ip = '127.0.0.1'

        if not self.red_port:
            self.red_port = 6379

        Mgv2.pool = redis.ConnectionPool(host = self.red_ip, port = self.red_port)



        # self.container_manager = KubernetesContainerManager(kubernetes_api_ip="eric-dev.clipper-k8s-dev.com",
        #                                                     useInternalIP=True)

        # self.container_manager = None

        # endpoints = self._k8s_v1.read_namespaced_endpoints(name="mgmt-frontend", namespace="default")
        # self.mgmt_ip = [addr.ip for addr in endpoints.subsets[0].addresses][0]



    @staticmethod
    def poll_for_query_frontend_Change():
        gevent.sleep(10)
        connection = redis.Redis(connection_pool=Mgv2.pool)
        dead_ips = connection.get("dead_ips")
        if dead_ips:
            dead_ips = json.loads(dead_ips)

        endpoints = Mgv2._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
        current_state = [addr.ip for addr in endpoints.subsets[0].addresses]
        query_frontend_ips = current_state

        logger.info("Original state is {}".format(current_state))

        while set(query_frontend_ips) == set(current_state):

            if dead_ips:
                for dead_ip in dead_ips:
                    zombie_models = json.loads(connection.get(json.dumps(dead_ip[0])))
                    if len(zombie_models) > 0:
                        Mgv2.reassign_models_to_new_containers(dead_ip[0], dead_ip[1], False)


            gevent.sleep(3)
            endpoints = Mgv2._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
            query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]

        gevent.sleep(10)
        endpoints = Mgv2._k8s_v1.read_namespaced_endpoints(name="query-frontend", namespace="default")
        query_frontend_ips = [addr.ip for addr in endpoints.subsets[0].addresses]
        logger.info("Current State is {}".format(query_frontend_ips))
        Mgv2.reassign_models_to_new_containers(list(set(current_state) - set(query_frontend_ips))[0],
                                               list(set(query_frontend_ips) - set(current_state))[0])

        # self.poll_for_query_frontend_Change()



    @staticmethod
    def poll_forever():

            while True:
                try:
                    Mgv2.poll_for_query_frontend_Change()
                except Exception as e:
                    print(e)


    @staticmethod
    def reassign_models_to_new_containers(lost_ip, new_ip, prnt = True):

        # while KubernetesContainerManager.model_lock.value:
        #     time.sleep(2)

        if prnt:
            logger.info("Query frontend has crashed: Old ip is {} and new ip is {}".format(lost_ip, new_ip))

        # with Mgv2.database_lock.get_lock():
        #     Mgv2.database_lock.value = 1


        connection = redis.Redis(connection_pool=Mgv2.pool)

        model_info = connection.get(json.dumps(lost_ip))

        keys = connection.keys('*')
        for key in keys:
            logger.info("Key is {} and value is {}".format(key, connection.get(key)))

        connection.set(json.dumps(lost_ip), json.dumps([]))

        model_info_lst = json.loads(model_info)

        logger.info("We are now going to redeploy {} models".format(len(model_info_lst)))

        for x in range(len(model_info_lst)):
            m = model_info_lst[x]

            Mgv2.container_manager.deploy_model(m["name"], m["version"], m["input_type"], m["image"], 1, new_ip)

        # with Mgv2.database_lock.get_lock():
        #     Mgv2.database_lock.value = 0



        dead = connection.get("dead_ips")
        if dead:
            value = json.loads(dead)
            if [lost_ip, new_ip] not in value:
                value.append([lost_ip, new_ip])
                value = json.dumps(value)
                connection.set("dead_ips", value)
        else:
            value = json.dumps([[lost_ip, new_ip]])
            connection.set("dead_ips", value)



    def dbg(self):
        last_time = time.time()

        while True:
            now = time.time() - last_time
            # print("Debug; Time is now {}", now)
            print("value of boolean is {}", Mgv2.database_lock.value)
            time.sleep(1)

    @staticmethod
    @app.route('/begin_polling', methods=['GET', 'POST'])
    def begin_polling():
        logger.info("Signal to start polling has been received")

        try:
            data = request.get_json(force = True)
            logger.info("Received data is {}".format(data))
            type = json.loads(data)['type']
            if type == 'kubernetes':
                Mgv2.container_manager = KubernetesContainerManager(kubernetes_api_ip="eric-dev.clipper-k8s-dev.com",
                                                                    useInternalIP=True)
            elif type == 'docker':
                Mgv2.container_manager = DockerContainerManager()

            Mgv2.container_manager.connect()

            logger.info("Starting polling")
            polling = Process(target=Mgv2.poll_forever)
            polling.start()
            return 'Success'
        except Exception as e:
            traceback.print_exc()
            return 'Failure'

    @staticmethod
    @app.route('/get_query_to_model_mapping', methods=['GET', 'POST'])
    def get_query_to_model_mapping():


        logger.info("Flask has received model info")

        try:
            query_to_model_mapping = request.get_json(force = True)


            if not isinstance(query_to_model_mapping, dict):
                query_to_model_mapping = json.loads(query_to_model_mapping)

            logger.info("Query_to_model_mapping: {}".format(query_to_model_mapping))

            # time.sleep(5)

            key = json.dumps(query_to_model_mapping["query_frontend"])
            value = json.dumps(query_to_model_mapping["model_info"])

            # logger.info("aaaaa")

            # while Mgv2.database_lock.value:
            #     time.sleep(2)

            connection = redis.Redis(connection_pool = Mgv2.pool)
            get_value = connection.get(key)

            # logger.info("Do you get here?")

            if get_value:
                get_list = json.loads(get_value)
                get_list.append(query_to_model_mapping["model_info"])
                value = json.dumps(get_list)
            else:
                get_list = [query_to_model_mapping["model_info"]]
                value = json.dumps(get_list)

            connection.set(key, value)

            logger.info("We just added {}, {} to the database".format(key, connection.get(key)))

            return "Success"
        except ValueError:
            traceback.print_exc()
            return "Failure"






if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--redis_ip",
                        nargs = '?',
                        default = None,
                        type=str)
    parser.add_argument("--redis_port",
                        nargs = '?',
                        default = None,
                        type=int)

    args = parser.parse_args()

    mgv2 = Mgv2(args.redis_ip, args.redis_port)

    # logger.info("Starting polling")
    # polling = Process(target=Mgv2.poll_forever)
    # polling.start()

    # debug = Process(target = mgv2.dbg)
    # debug.start()

    # connection = redis.Redis(connection_pool=Mgv2.pool)
    # connection.set("test_key", "test_value")
    # print(connection.get("test_key"))

    logger.info("Flask App is starting")

    # logger.info("Setting flask IP to {}".format(mgv2.mgmt_ip))

    # Mgv2.app.run(host = mgv2.mgmt_ip, port=5000)
    Mgv2.app.run(host='0.0.0.0', port=5000)








