import unittest
import sys
import os
import json
import time
import requests
from sklearn import svm
from argparse import ArgumentParser
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath('%s/../' % cur_dir))
from clipper_admin.clipper_manager import Clipper
import random
import socket
"""
Executes a test suite consisting of two separate cases: short tests and long tests.
Before each case, an instance of Clipper is created. Tests
are then performed by invoking methods on this instance, often resulting
in the execution of docker commands.
"""

# range of ports where available ports can be found
PORT_RANGE = [34256, 40000]


def find_unbound_port():
    """
    Returns an unbound port number on 127.0.0.1.
    """
    while True:
        port = random.randint(*PORT_RANGE)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.bind(("127.0.0.1", port))
            return port
        except socket.error:
            print("randomly generated port %d is bound. Trying again." % port)


class ClipperManagerTestCaseShort(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        self.clipper_inst = Clipper(
            "localhost", redis_port=find_unbound_port())
        self.clipper_inst.stop_all()
        self.clipper_inst.start()
        self.app_name = "app1"
        self.model_name = "m1"
        self.model_version_1 = 1
        self.model_version_2 = 2
        self.deploy_model_name = "m3"
        self.deploy_model_version = 1

    @classmethod
    def tearDownClass(self):
        self.clipper_inst.stop_all()

    def test_external_models_register_correctly(self):
        name = "m1"
        version1 = 1
        input_type = "doubles"
        result = self.clipper_inst.register_external_model(
            self.model_name, self.model_version_1, input_type)
        self.assertTrue(result)
        registered_model_info = self.clipper_inst.get_model_info(
            self.model_name, self.model_version_1)
        self.assertIsNotNone(registered_model_info)

        version2 = 2
        result = self.clipper_inst.register_external_model(
            self.model_name, self.model_version_2, input_type)
        self.assertTrue(result)
        registered_model_info = self.clipper_inst.get_model_info(
            self.model_name, self.model_version_2)
        self.assertIsNotNone(registered_model_info)

    def test_application_registers_correctly(self):
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        self.clipper_inst.register_application(self.app_name, self.model_name,
                                               input_type, default_output,
                                               slo_micros)
        registered_applications = self.clipper_inst.get_all_apps()
        self.assertGreaterEqual(len(registered_applications), 1)
        self.assertTrue(self.app_name in registered_applications)

    def get_app_info_for_registered_app_returns_info_dictionary(self):
        result = self.clipper_inst.get_app_info(self.app_name)
        self.assertIsNotNone(result)
        self.assertEqual(type(result), dict)

    def get_app_info_for_nonexistent_app_returns_none(self):
        result = self.clipper_inst.get_app_info("fake_app")
        self.assertIsNone(result)

    def test_add_container_for_external_model_fails(self):
        result = self.clipper_inst.add_container(self.model_name,
                                                 self.model_version_1)
        self.assertFalse(result)

    def test_model_version_sets_correctly(self):
        self.clipper_inst.set_model_version(self.model_name,
                                            self.model_version_1)
        all_models = self.clipper_inst.get_all_models(verbose=True)
        models_list_contains_correct_version = False
        for model_info in all_models:
            version = model_info["model_version"]
            if version == self.model_version_1:
                models_list_contains_correct_version = True
                self.assertTrue(model_info["is_current_version"])

        self.assertTrue(models_list_contains_correct_version)

    def test_get_logs_creates_log_files(self):
        log_file_names = self.clipper_inst.get_clipper_logs()
        self.assertIsNotNone(log_file_names)
        self.assertGreaterEqual(len(log_file_names), 1)
        for file_name in log_file_names:
            self.assertTrue(os.path.isfile(file_name))

    def test_inspect_instance_returns_json_dict(self):
        metrics = self.clipper_inst.inspect_instance()
        self.assertEqual(type(metrics), dict)
        self.assertGreaterEqual(len(metrics), 1)

    def test_model_deploys_successfully(self):
        # Initialize a support vector classifier 
        # that will be deployed to a no-op container
        model_data = svm.SVC()
        container_name = "clipper/noop-container"
        input_type = "doubles"
        result = self.clipper_inst.deploy_model(
            self.deploy_model_name, self.deploy_model_version, model_data,
            container_name, input_type)
        self.assertTrue(result)
        model_info = self.clipper_inst.get_model_info(
            self.deploy_model_name, self.deploy_model_version)
        self.assertIsNotNone(model_info)
        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/noop-container\"")
        self.assertIsNotNone(running_containers_output)
        self.assertGreaterEqual(len(running_containers_output), 1)

    def test_add_container_for_deployed_model_succeeds(self):
        result = self.clipper_inst.add_container(self.deploy_model_name,
                                                 self.deploy_model_version)
        self.assertTrue(result)
        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/noop-container\"")
        self.assertIsNotNone(running_containers_output)
        split_output = running_containers_output.split("\n")
        self.assertGreaterEqual(len(split_output), 2)

    def test_remove_inactive_containers_succeeds(self):
        # Initialize a support vector classifier 
        # that will be deployed to a no-op container
        self.clipper_inst.stop_all()
        self.clipper_inst.start()
        model_data = svm.SVC()
        container_name = "clipper/noop-container"
        input_type = "doubles"
        model_name = "remove_inactive_test_model"
        result = self.clipper_inst.deploy_model(
            model_name,
            1,
            model_data,
            container_name,
            input_type,
            num_containers=2)
        self.assertTrue(result)
        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/noop-container\"")
        self.assertIsNotNone(running_containers_output)
        num_running_containers = running_containers_output.split("\n")
        print("RUNNING CONTAINERS: %s" % str(num_running_containers))
        self.assertEqual(len(num_running_containers), 2)

        result = self.clipper_inst.deploy_model(
            model_name,
            2,
            model_data,
            container_name,
            input_type,
            num_containers=3)
        self.assertTrue(result)
        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/noop-container\"")
        self.assertIsNotNone(running_containers_output)
        num_running_containers = running_containers_output.split("\n")
        self.assertEqual(len(num_running_containers), 5)

        num_containers_removed = self.clipper_inst.remove_inactive_containers(
            model_name)
        self.assertEqual(num_containers_removed, 2)
        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/noop-container\"")
        self.assertIsNotNone(running_containers_output)
        num_running_containers = running_containers_output.split("\n")
        self.assertEqual(len(num_running_containers), 3)

    def test_predict_function_deploys_successfully(self):
        model_name = "m2"
        model_version = 1
        predict_func = lambda inputs: ["0" for x in inputs]
        input_type = "doubles"
        result = self.clipper_inst.deploy_predict_function(
            model_name, model_version, predict_func, input_type)
        self.assertTrue(result)
        model_info = self.clipper_inst.get_model_info(model_name,
                                                      model_version)
        self.assertIsNotNone(model_info)
        running_containers_output = self.clipper_inst._execute_standard(
            "docker ps -q --filter \"ancestor=clipper/python-container\"")
        self.assertIsNotNone(running_containers_output)
        self.assertGreaterEqual(len(running_containers_output), 1)


class ClipperManagerTestCaseLong(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        self.clipper_inst = Clipper(
            "localhost", redis_port=find_unbound_port())
        self.clipper_inst.stop_all()
        self.clipper_inst.start()
        self.app_name_1 = "app3"
        self.app_name_2 = "app4"
        self.model_name_1 = "m4"
        self.model_name_2 = "m5"
        self.input_type = "doubles"
        self.default_output = "DEFAULT"
        self.latency_slo_micros = 30000
        self.clipper_inst.register_application(
            self.app_name_1, self.model_name_1, self.input_type,
            self.default_output, self.latency_slo_micros)
        self.clipper_inst.register_application(
            self.app_name_2, self.model_name_2, self.input_type,
            self.default_output, self.latency_slo_micros)

    @classmethod
    def tearDownClass(self):
        self.clipper_inst.stop_all()

    def test_deployed_model_queried_successfully(self):
        model_version = 1
        # Initialize a support vector classifier 
        # that will be deployed to a no-op container
        model_data = svm.SVC()
        container_name = "clipper/noop-container"
        result = self.clipper_inst.deploy_model(
            self.model_name_2, model_version, model_data, container_name,
            self.input_type)
        self.assertTrue(result)

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

    def test_deployed_predict_function_queried_successfully(self):
        model_version = 1
        predict_func = lambda inputs: [str(len(x)) for x in inputs]
        input_type = "doubles"
        result = self.clipper_inst.deploy_predict_function(
            self.model_name_1, model_version, predict_func, input_type)
        self.assertTrue(result)

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
                self.assertEqual(int(output), len(test_input))
                break

        self.assertTrue(received_non_default_prediction)


SHORT_TEST_ORDERING = [
    'test_external_models_register_correctly',
    'test_application_registers_correctly',
    'get_app_info_for_registered_app_returns_info_dictionary',
    'get_app_info_for_nonexistent_app_returns_none',
    'test_add_container_for_external_model_fails',
    'test_model_version_sets_correctly', 'test_get_logs_creates_log_files',
    'test_inspect_instance_returns_json_dict',
    'test_model_deploys_successfully',
    'test_add_container_for_deployed_model_succeeds',
    'test_remove_inactive_containers_succeeds',
    'test_predict_function_deploys_successfully'
]

LONG_TEST_ORDERING = [
    'test_deployed_model_queried_successfully',
    'test_deployed_predict_function_queried_successfully'
]

if __name__ == '__main__':
    description = "Runs clipper manager tests. If no arguments are specified, all tests are executed."
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
