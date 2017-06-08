"""
Executes a test suite consisting of two separate cases: short tests and long tests.
Before each case, an instance of Clipper is created. Tests
are then performed by invoking methods on this instance, often resulting
in the execution of docker commands.
"""

from __future__ import absolute_import, division, print_function
import unittest
import sys
import os
import json
import time
import requests
from argparse import ArgumentParser
import logging
import random
import socket
from test_utils import get_docker_client, create_container_manager, fake_model_data

cur_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.abspath('%s/../clipper_admin_v2' % cur_dir))
import clipper_admin as cl
from clipper_admin.deployers.python import deploy_python_closure
from clipper_admin import __version__ as code_version

sys.path.insert(0, os.path.abspath('%s/util_direct_import/' % cur_dir))
from util_package import mock_module_in_package as mmip
import mock_module as mm


logging.basicConfig(format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
                    datefmt='%y-%m-%d:%H:%M:%S',
                    level=logging.INFO)

logger = logging.getLogger(__name__)


class ClipperManagerTestCaseShort(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        self.cm_type = "docker"
        self.cm = create_container_manager(self.cm_type, cleanup=True, start_clipper=True)
        self.app_name = "app1"
        self.model_name = "m1"
        self.model_version_1 = 1
        self.model_version_2 = 2
        self.deploy_model_name = "m3"
        self.deploy_model_version = 1

    @classmethod
    def tearDownClass(self):
        self.cm = create_container_manager(self.cm_type, cleanup=True, start_clipper=False)

    def test_register_model_correct(self):
        input_type = "doubles"
        cl.register_model(self.cm,
                          self.model_name,
                          self.model_version_1,
                          input_type)
        registered_model_info = cl.get_model_info(self.cm,
                                                  self.model_name,
                                                  self.model_version_1)
        self.assertIsNotNone(registered_model_info)

        cl.register_model(self.cm,
                          self.model_name,
                          self.model_version_2,
                          input_type)
        registered_model_info = cl.get_model_info(self.cm,
                                                  self.model_name,
                                                  self.model_version_2)
        self.assertIsNotNone(registered_model_info)

    def test_register_application_correct(self):
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        cl.register_application(self.cm,
                                self.app_name,
                                self.model_name,
                                input_type,
                                default_output,
                                slo_micros)
        registered_applications = cl.get_all_apps(self.cm)
        self.assertGreaterEqual(len(registered_applications), 1)
        self.assertTrue(self.app_name in registered_applications)
        #################

    def test_link_not_registered_model_to_app_fails(self):
        not_deployed_model = "test_model"
        result = self.clipper_inst.link_model_to_app(self.app_name,
                                                     not_deployed_model)
        self.assertFalse(result)

    def test_get_model_links_when_none_exist_returns_empty_list(self):
        result = self.clipper_inst.get_linked_models(self.app_name)
        self.assertEqual([], result)

    def test_link_registered_model_to_app_succeeds(self):
        result = self.clipper_inst.link_model_to_app(self.app_name,
                                                     self.model_name)
        self.assertTrue(result)

    def test_get_model_links_returns_list_of_linked_models(self):
        result = self.clipper_inst.get_linked_models(self.app_name)
        self.assertEqual([self.model_name], result)

#############
    def get_app_info_for_registered_app_returns_info_dictionary(self):
        result = cl.get_app_info(self.cm, self.app_name)
        self.assertIsNotNone(result)
        self.assertEqual(type(result), dict)

    def get_app_info_for_nonexistent_app_returns_none(self):
        result = cl.get_app_info(self.cm, "fake_app")
        self.assertIsNone(result)

    def test_add_replica_for_external_model_fails(self):
        with self.assertRaises(cl.ClipperException) as context:
            cl.add_replica(self.cm,
                           self.model_name,
                           self.model_version_1)
        self.assertTrue("containerless model" in str(context.exception))

    def test_model_version_sets_correctly(self):
        cl.set_model_version(self.cm,
                             self.model_name,
                             self.model_version_1)
        all_models = cl.get_all_models(self.cm, verbose=True)
        models_list_contains_correct_version = False
        for model_info in all_models:
            version = model_info["model_version"]
            if version == str(self.model_version_1):
                models_list_contains_correct_version = True
                self.assertTrue(model_info["is_current_version"])

        self.assertTrue(models_list_contains_correct_version)

    def test_get_logs_creates_log_files(self):
        log_file_names = cl.get_clipper_logs(self.cm)
        self.assertIsNotNone(log_file_names)
        self.assertGreaterEqual(len(log_file_names), 1)
        for file_name in log_file_names:
            self.assertTrue(os.path.isfile(file_name))

    def test_inspect_instance_returns_json_dict(self):
        metrics = cl.inspect_instance(self.cm)
        self.assertEqual(type(metrics), dict)
        self.assertGreaterEqual(len(metrics), 1)

    def test_model_deploys_successfully(self):
        container_name = "clipper/noop-container:{}".format(code_version)
        input_type = "doubles"
        cl.deploy_model(self.cm,
                        self.deploy_model_name,
                        self.deploy_model_version,
                        input_type,
                        fake_model_data,
                        container_name)
        model_info = cl.get_model_info(self.cm, self.deploy_model_name, self.deploy_model_version)
        self.assertIsNotNone(model_info)
        self.assertEqual(type(model_info), dict)
        docker_client = get_docker_client()
        containers = docker_client.containers.list(
            filters={"ancestor": container_name})
        self.assertGreaterEqual(len(containers), 1)

    def test_add_replica_for_deployed_model_succeeds(self):
        cl.add_replica(self.cm,
                       self.deploy_model_name,
                       self.deploy_model_version)
        container_name = "clipper/noop-container"
        docker_client = get_docker_client()
        containers = docker_client.containers.list(
            filters={"ancestor": container_name})
        self.assertGreaterEqual(len(containers), 2)

    def test_remove_inactive_containers_succeeds(self):
        self.cm = create_container_manager(self.cm_type, cleanup=True, start_clipper=True)
        container_name = "clipper/noop-container"
        input_type = "doubles"
        model_name = "remove_inactive_test_model"
        cl.deploy_model(self.cm,
                        model_name,
                        1,
                        input_type,
                        fake_model_data,
                        container_name,
                        num_replicas=2)
        docker_client = get_docker_client()
        containers = docker_client.containers.list(
            filters={"ancestor": container_name})
        self.assertEqual(len(containers), 2)

        cl.deploy_model(self.cm,
                        model_name,
                        2,
                        input_type,
                        fake_model_data,
                        container_name,
                        num_replicas=3)
        containers = docker_client.containers.list(
            filters={"ancestor": container_name})
        self.assertEqual(len(containers), 5)

        cl.stop_inactive_model_versions(
            self.cm, model_name)
        containers = docker_client.containers.list(
            filters={"ancestor": container_name})
        self.assertEqual(len(containers), 3)

    def test_python_closure_deploys_successfully(self):
        model_name = "m2"
        model_version = 1

        def predict_func(inputs):
            return ["0" for x in inputs]

        input_type = "doubles"
        deploy_python_closure(self.cm,
                              model_name,
                              model_version,
                              input_type,
                              predict_func)
        model_info = cl.get_model_info(self.cm,
                                       model_name,
                                       model_version)
        self.assertIsNotNone(model_info)

        docker_client = get_docker_client()
        containers = docker_client.containers.list(
            filters={"ancestor": "clipper/python-closure-container"})
        self.assertGreaterEqual(len(containers), 1)

    def test_register_app_and_deploy_predict_function_is_successful(self):
        # TODO(rebase): update this function
        model_version = 1
        app_and_model_name = "easy_register_app_model"
        predict_func = lambda inputs: ["0" for x in inputs]
        input_type = "doubles"

        result = self.clipper_inst.register_app_and_deploy_predict_function(
            app_and_model_name, predict_func, input_type)

        self.assertTrue(result)
        model_info = self.clipper_inst.get_model_info(
            app_and_model_name,
            clipper_admin.clipper_manager.DEFAULT_MODEL_VERSION)
        self.assertIsNotNone(model_info)

        linked_models = self.clipper_inst.get_linked_models(app_and_model_name)
        self.assertIsNotNone(linked_models)

        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/python-container:{}\"".
            format(code_version))
        self.assertIsNotNone(running_containers_output)
        self.assertGreaterEqual(len(running_containers_output), 2)


class ClipperManagerTestCaseLong(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        self.cm = create_container_manager(self.cm_type, cleanup=True, start_clipper=True)
        self.app_name_1 = "app3"
        self.app_name_2 = "app4"
        self.model_name_1 = "m4"
        self.model_name_2 = "m5"
        self.input_type = "doubles"
        self.default_output = "DEFAULT"
        self.latency_slo_micros = 30000

        cl.register_application(self.cm,
                                self.app_name_1,
                                self.model_name_1,
                                self.input_type,
                                self.default_output,
                                self.latency_slo_micros)

        cl.register_application(self.cm,
                                self.app_name_2,
                                self.model_name_2,
                                self.input_type,
                                self.default_output,
                                self.latency_slo_micros)

    @classmethod
    def tearDownClass(self):
        self.cm = create_container_manager(self.cm_type, cleanup=True, start_clipper=False)

    def test_queries_to_app_without_linked_models_yield_default_predictions(
            self):
        url = "http://localhost:1337/{}/predict".format(self.app_name_2)
        test_input = [99.3, 18.9, 67.2, 34.2]
        req_json = json.dumps({'input': test_input})
        headers = {'Content-type': 'application/json'}
        response = requests.post(url, headers=headers, data=req_json)
        parsed_response = response.json()
        print(parsed_response)
        self.assertEqual(parsed_response["output"], self.default_output)
        self.assertTrue(parsed_response["default"])

    def test_deployed_model_queried_successfully(self):
        model_version = 1
        container_name = "clipper/noop-container"
        cl.deploy_model(self.cm,
                        self.model_name_2,
                        model_version,
                        self.input_type,
                        fake_model_data,
                        container_name)

        ### Link model and app
        self.clipper_inst.link_model_to_app(self.app_name_2, self.model_name_2)
        time.sleep(30)

        url = "http://localhost:1337/{}/predict".format(self.app_name_2)
        test_input = [99.3, 18.9, 67.2, 34.2]
        req_json = json.dumps({'input': test_input})
        headers = {'Content-type': 'application/json'}
        response = requests.post(url, headers=headers, data=req_json)
        parsed_response = response.json()
        print(parsed_response)
        self.assertNotEqual(parsed_response["output"], self.default_output)
        self.assertFalse(parsed_response["default"])

    def test_deployed_python_closure_queried_successfully(self):
        model_version = 1

        def predict_func(inputs):
            return [str(len(x)) for x in inputs]

        input_type = "doubles"
        deploy_python_closure(self.cm,
                              self.model_name_1,
                              model_version,
                              input_type,
                              predict_func)

        self.clipper_inst.link_model_to_app(self.app_name_1, self.model_name_1)
        time.sleep(60)

        received_non_default_prediction = False
        url = "http://localhost:1337/{}/predict".format(self.app_name_1)
        test_input = [101.1, 99.5, 107.2]
        req_json = json.dumps({'input': test_input})
        headers = {'Content-type': 'application/json'}
        for i in range(0, 40):
            response = requests.post(url, headers=headers, data=req_json)
            parsed_response = response.json()
            print(parsed_response)
            output = parsed_response["output"]
            if output == self.default_output:
                time.sleep(20)
            else:
                received_non_default_prediction = True
                self.assertEqual(
                    int(output),
                    mm.COEFFICIENT * mmip.COEFFICIENT * len(test_input))
                break

        self.assertTrue(received_non_default_prediction)


SHORT_TEST_ORDERING = [
    'test_register_model_correct',
    'test_register_application_correct',
    'test_link_not_registered_model_to_app_fails',
    'test_get_model_links_when_none_exist_returns_empty_list',
    'test_link_registered_model_to_app_succeeds',
    'test_get_model_links_returns_list_of_linked_models',
    'get_app_info_for_registered_app_returns_info_dictionary',
    'get_app_info_for_nonexistent_app_returns_none',
    'test_add_replica_for_external_model_fails',
    'test_model_version_sets_correctly',
    'test_get_logs_creates_log_files',
    'test_inspect_instance_returns_json_dict',
    'test_model_deploys_successfully',
    'test_add_replica_for_deployed_model_succeeds',
    'test_remove_inactive_containers_succeeds',
    'test_python_closure_deploys_successfully',
    'test_register_app_and_deploy_predict_function_is_successful',
]

LONG_TEST_ORDERING = [
    'test_queries_to_app_without_linked_models_yield_default_predictions',
    'test_deployed_and_linked_model_queried_successfully',
    'test_deployed_and_linked_predict_function_queried_successfully'
    # 'test_deployed_model_queried_successfully',
    # 'test_deployed_python_closure_queried_successfully'
]

if __name__ == '__main__':
    description = ("Runs clipper manager tests. If no arguments are specified, all tests are "
                   "executed.")
    parser = ArgumentParser(description)
    parser.add_argument(
        "-s",
        "--short",
        action="store_true",
        dest="run_short",
        help="Run the short suite of test cases")
    parser.add_argument(
        "-l",
        "--long",
        action="store_true",
        dest="run_long",
        help="Run the long suite of test cases")
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        dest="run_all",
        help="Run all test cases")
    args = parser.parse_args()

    # If neither the short nor the long argument is specified,
    # we will run all tests
    args.run_all = args.run_all or ((not args.run_short) and
                                    (not args.run_long))

    suite = unittest.TestSuite()

    if args.run_short or args.run_all:
        for test in SHORT_TEST_ORDERING:
            suite.addTest(ClipperManagerTestCaseShort(test))

    if args.run_long or args.run_all:
        for test in LONG_TEST_ORDERING:
            suite.addTest(ClipperManagerTestCaseLong(test))

    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(not result.wasSuccessful())
