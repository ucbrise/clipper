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
import tempfile
import shutil
from argparse import ArgumentParser
import logging
from test_utils import get_docker_client, create_docker_connection, fake_model_data

cur_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.abspath('%s/../clipper_admin' % cur_dir))
import clipper_admin as cl
from clipper_admin.deployers.python import create_endpoint as create_py_endpoint
from clipper_admin.deployers.python import deploy_python_closure
from clipper_admin import __version__ as clipper_version

sys.path.insert(0, os.path.abspath('%s/util_direct_import/' % cur_dir))
from util_package import mock_module_in_package as mmip
import mock_module as mm

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)


class ClipperManagerTestCaseShort(unittest.TestCase):
    @classmethod
    def tearDownClass(self):
        self.clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False)

    def setUp(self):
        self.clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)

    def test_register_model_correct(self):
        input_type = "doubles"
        model_name = "m"
        self.clipper_conn.register_model(model_name, "v1", input_type)
        registered_model_info = self.clipper_conn.get_model_info(
            model_name, "v1")
        self.assertIsNotNone(registered_model_info)

        self.clipper_conn.register_model(model_name, "v2", input_type)
        registered_model_info = self.clipper_conn.get_model_info(
            model_name, "v2")
        self.assertIsNotNone(registered_model_info)

    def test_register_application_correct(self):
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        app_name = "testapp"
        self.clipper_conn.register_application(app_name, input_type,
                                               default_output, slo_micros)
        registered_applications = self.clipper_conn.get_all_apps()
        self.assertGreaterEqual(len(registered_applications), 1)
        self.assertTrue(app_name in registered_applications)

    def test_link_not_registered_model_to_app_fails(self):
        not_deployed_model = "test_model"
        app_name = "testapp"
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        self.clipper_conn.register_application(app_name, input_type,
                                               default_output, slo_micros)
        with self.assertRaises(cl.ClipperException) as context:
            self.clipper_conn.link_model_to_app(app_name, not_deployed_model)
        self.assertTrue("No model with name" in str(context.exception))

    def test_get_model_links_when_none_exist_returns_empty_list(self):
        app_name = "testapp"
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        self.clipper_conn.register_application(app_name, input_type,
                                               default_output, slo_micros)
        result = self.clipper_conn.get_linked_models(app_name)
        self.assertEqual([], result)

    def test_link_registered_model_to_app_succeeds(self):
        # Register app
        app_name = "testapp"
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        self.clipper_conn.register_application(app_name, input_type,
                                               default_output, slo_micros)

        # Register model
        model_name = "m"
        self.clipper_conn.register_model(model_name, "v1", input_type)

        self.clipper_conn.link_model_to_app(app_name, model_name)
        result = self.clipper_conn.get_linked_models(app_name)
        self.assertEqual([model_name], result)

    def get_app_info_for_registered_app_returns_info_dictionary(self):
        # Register app
        app_name = "testapp"
        input_type = "doubles"
        default_output = "DEFAULT"
        slo_micros = 30000
        self.clipper_conn.register_application(app_name, input_type,
                                               default_output, slo_micros)
        result = self.clipper_conn.get_app_info(app_name)
        self.assertIsNotNone(result)
        self.assertEqual(type(result), dict)

    def get_app_info_for_nonexistent_app_returns_none(self):
        result = self.clipper_conn.get_app_info("fake_app")
        self.assertIsNone(result)

    def test_set_num_replicas_for_external_model_fails(self):
        # Register model
        model_name = "m"
        input_type = "doubles"
        version = "v1"
        self.clipper_conn.register_model(model_name, version, input_type)
        with self.assertRaises(cl.ClipperException) as context:
            self.clipper_conn.set_num_replicas(model_name, 5, version)
        self.assertTrue("containerless model" in str(context.exception))

    def test_model_version_sets_correctly(self):
        model_name = "m"
        input_type = "doubles"

        v1 = "v1"
        self.clipper_conn.register_model(model_name, v1, input_type)

        v2 = "v2"
        self.clipper_conn.register_model(model_name, v2, input_type)

        self.clipper_conn.set_model_version(model_name, v1)
        all_models = self.clipper_conn.get_all_models(verbose=True)
        models_list_contains_correct_version = False
        for model_info in all_models:
            version = model_info["model_version"]
            if version == v1:
                models_list_contains_correct_version = True
                self.assertTrue(model_info["is_current_version"])
        self.assertTrue(models_list_contains_correct_version)

    def test_get_logs_creates_log_files(self):
        if not os.path.exists(cl.CLIPPER_TEMP_DIR):
            os.makedirs(cl.CLIPPER_TEMP_DIR)
        tmp_log_dir = tempfile.mkdtemp(dir=cl.CLIPPER_TEMP_DIR)
        log_file_names = self.clipper_conn.get_clipper_logs(
            logging_dir=tmp_log_dir)
        self.assertIsNotNone(log_file_names)
        self.assertGreaterEqual(len(log_file_names), 1)
        for file_name in log_file_names:
            self.assertTrue(os.path.isfile(file_name))

        # Remove temp files
        shutil.rmtree(tmp_log_dir)

    def test_inspect_instance_returns_json_dict(self):
        metrics = self.clipper_conn.inspect_instance()
        self.assertEqual(type(metrics), dict)
        self.assertGreaterEqual(len(metrics), 1)

    def test_model_deploys_successfully(self):
        model_name = "m"
        version = "v1"
        container_name = "clipper/noop-container:{}".format(clipper_version)
        input_type = "doubles"
        self.clipper_conn.build_and_deploy_model(
            model_name, version, input_type, fake_model_data, container_name)
        model_info = self.clipper_conn.get_model_info(model_name, version)
        self.assertIsNotNone(model_info)
        self.assertEqual(type(model_info), dict)
        docker_client = get_docker_client()
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), 1)

    def test_set_num_replicas_for_deployed_model_succeeds(self):
        model_name = "set-num-reps-model"
        input_type = "doubles"
        version = "v1"
        container_name = "clipper/noop-container:{}".format(clipper_version)
        input_type = "doubles"
        self.clipper_conn.build_and_deploy_model(
            model_name, version, input_type, fake_model_data, container_name)

        # Version defaults to current version
        self.clipper_conn.set_num_replicas(model_name, 4)
        time.sleep(1)
        num_reps = self.clipper_conn.get_num_replicas(model_name, version)
        self.assertEqual(num_reps, 4)

        self.clipper_conn.set_num_replicas(model_name, 2, version)
        time.sleep(1)
        num_reps = self.clipper_conn.get_num_replicas(model_name, version)
        self.assertEqual(num_reps, 2)

    def test_remove_inactive_containers_succeeds(self):
        container_name = "clipper/noop-container:{}".format(clipper_version)
        input_type = "doubles"
        model_name = "remove-inactive-test-model"
        self.clipper_conn.build_and_deploy_model(
            model_name,
            1,
            input_type,
            fake_model_data,
            container_name,
            num_replicas=2)
        docker_client = get_docker_client()
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), 2)

        self.clipper_conn.build_and_deploy_model(
            model_name,
            2,
            input_type,
            fake_model_data,
            container_name,
            num_replicas=3)
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), 5)

        self.clipper_conn.stop_inactive_model_versions([model_name])
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), 3)

    def test_stop_models(self):
        container_name = "clipper/noop-container:{}".format(clipper_version)
        input_type = "doubles"
        mnames = ["jimmypage", "robertplant", "jpj", "johnbohnam"]
        versions = ["i", "ii", "iii", "iv"]
        for model_name in mnames:
            for version in versions:
                self.clipper_conn.deploy_model(
                    model_name,
                    version,
                    input_type,
                    container_name,
                    num_replicas=1)

        docker_client = get_docker_client()
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), len(mnames) * len(versions))

        # stop all versions of models jimmypage, robertplant
        self.clipper_conn.stop_models(mnames[:2])
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), len(mnames[2:]) * len(versions))

        # After calling this method, the remaining models should be:
        # jpj:i, jpj:iii, johnbohman:ii
        self.clipper_conn.stop_versioned_models({
            "jpj": ["ii", "iv"],
            "johnbohnam": ["i", "iv", "iii"],
        })
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), 3)

        self.clipper_conn.stop_all_model_containers()
        containers = docker_client.containers.list(filters={
            "ancestor": container_name
        })
        self.assertEqual(len(containers), 0)

    def test_python_closure_deploys_successfully(self):
        model_name = "m2"
        model_version = 1

        def predict_func(inputs):
            return ["0" for x in inputs]

        input_type = "doubles"
        deploy_python_closure(self.clipper_conn, model_name, model_version,
                              input_type, predict_func)
        model_info = self.clipper_conn.get_model_info(model_name,
                                                      model_version)
        self.assertIsNotNone(model_info)

        docker_client = get_docker_client()
        containers = docker_client.containers.list(
            filters={
                "ancestor":
                "clipper/python-closure-container:{}".format(clipper_version)
            })
        self.assertGreaterEqual(len(containers), 1)

    def test_register_py_endpoint(self):
        name = "py-closure-test"
        expected_version = 1

        def predict_func(inputs):
            return ["0" for x in inputs]

        input_type = "doubles"

        create_py_endpoint(self.clipper_conn, name, input_type, predict_func)

        registered_applications = self.clipper_conn.get_all_apps()
        self.assertEqual(len(registered_applications), 1)
        self.assertTrue(name in registered_applications)

        registered_model_info = self.clipper_conn.get_model_info(
            name, expected_version)
        self.assertIsNotNone(registered_model_info)

        linked_models = self.clipper_conn.get_linked_models(name)
        self.assertIsNotNone(linked_models)

        docker_client = get_docker_client()
        containers = docker_client.containers.list(
            filters={
                "ancestor":
                "clipper/python-closure-container:{}".format(clipper_version)
            })
        self.assertEqual(len(containers), 1)


class ClipperManagerTestCaseLong(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        self.clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)
        self.app_name_1 = "app3"
        self.app_name_2 = "app4"
        self.app_name_3 = "app5"
        self.app_name_4 = "app6"
        self.model_name_1 = "m4"
        self.model_name_2 = "m5"
        self.model_name_3 = "m6"
        self.model_name_4 = "m7"
        self.input_type = "doubles"
        self.default_output = "DEFAULT"
        self.latency_slo_micros = 30000

        self.clipper_conn.register_application(
            self.app_name_1, self.input_type, self.default_output,
            self.latency_slo_micros)

        self.clipper_conn.register_application(
            self.app_name_2, self.input_type, self.default_output,
            self.latency_slo_micros)

        self.clipper_conn.register_application(
            self.app_name_3, self.input_type, self.default_output,
            self.latency_slo_micros)

        self.clipper_conn.register_application(
            self.app_name_4, self.input_type, self.default_output,
            self.latency_slo_micros)

    @classmethod
    def tearDownClass(self):
        self.clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False)

    def test_unlinked_app_returns_default_predictions(self):
        addr = self.clipper_conn.get_query_addr()
        url = "http://{addr}/{app}/predict".format(
            addr=addr, app=self.app_name_2)
        test_input = [99.3, 18.9, 67.2, 34.2]
        req_json = json.dumps({'input': test_input})
        headers = {'Content-type': 'application/json'}
        response = requests.post(url, headers=headers, data=req_json)
        parsed_response = response.json()
        logger.info(parsed_response)
        self.assertEqual(parsed_response["output"], self.default_output)
        self.assertTrue(parsed_response["default"])

    def test_deployed_model_queried_successfully(self):
        model_version = 1
        container_name = "clipper/noop-container:{}".format(clipper_version)
        self.clipper_conn.build_and_deploy_model(
            self.model_name_2, model_version, self.input_type, fake_model_data,
            container_name)

        self.clipper_conn.link_model_to_app(self.app_name_2, self.model_name_2)
        time.sleep(30)
        addr = self.clipper_conn.get_query_addr()
        url = "http://{addr}/{app}/predict".format(
            addr=addr, app=self.app_name_2)
        test_input = [99.3, 18.9, 67.2, 34.2]
        req_json = json.dumps({'input': test_input})
        headers = {'Content-type': 'application/json'}
        response = requests.post(url, headers=headers, data=req_json)
        parsed_response = response.json()
        logger.info(parsed_response)
        self.assertNotEqual(parsed_response["output"], self.default_output)
        self.assertFalse(parsed_response["default"])

    def test_batch_queries_returned_successfully(self):
        model_version = 1
        container_name = "clipper/noop-container:{}".format(clipper_version)
        self.clipper_conn.build_and_deploy_model(
            self.model_name_3, model_version, self.input_type, fake_model_data,
            container_name)

        self.clipper_conn.link_model_to_app(self.app_name_3, self.model_name_3)
        time.sleep(30)
        addr = self.clipper_conn.get_query_addr()
        url = "http://{addr}/{app}/predict".format(
            addr=addr, app=self.app_name_3)
        test_input = [[99.3, 18.9, 67.2, 34.2], [101.1, 45.6, 98.0, 99.1], \
                      [12.3, 6.7, 42.1, 12.6], [9.01, 87.6, 70.2, 19.6]]
        req_json = json.dumps({'input_batch': test_input})
        headers = {'Content-type': 'application/json'}
        response = requests.post(url, headers=headers, data=req_json)
        parsed_response = response.json()
        logger.info(parsed_response)
        self.assertEqual(
            len(parsed_response["batch_predictions"]), len(test_input))

    def test_deployed_python_closure_queried_successfully(self):
        model_version = 1

        def predict_func(inputs):
            return [
                str(mm.COEFFICIENT * mmip.COEFFICIENT * len(x)) for x in inputs
            ]

        input_type = "doubles"
        deploy_python_closure(self.clipper_conn, self.model_name_1,
                              model_version, input_type, predict_func)

        self.clipper_conn.link_model_to_app(self.app_name_1, self.model_name_1)
        time.sleep(60)

        received_non_default_prediction = False
        addr = self.clipper_conn.get_query_addr()
        url = "http://{addr}/{app}/predict".format(
            addr=addr, app=self.app_name_1)
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

    def test_fixed_batch_size_model_processes_specified_query_batch_size_when_saturated(
            self):
        model_version = 1

        def predict_func(inputs):
            batch_size = len(inputs)
            return [str(batch_size) for _ in inputs]

        fixed_batch_size = 9
        total_num_queries = fixed_batch_size * 50
        deploy_python_closure(
            self.clipper_conn,
            self.model_name_4,
            model_version,
            self.input_type,
            predict_func,
            batch_size=fixed_batch_size)
        self.clipper_conn.link_model_to_app(self.app_name_4, self.model_name_4)
        time.sleep(60)

        addr = self.clipper_conn.get_query_addr()
        url = "http://{addr}/{app}/predict".format(
            addr=addr, app=self.app_name_4)
        test_input = [[float(x) + (j * .001) for x in range(5)]
                      for j in range(total_num_queries)]
        req_json = json.dumps({'input_batch': test_input})
        headers = {'Content-type': 'application/json'}
        response = requests.post(url, headers=headers, data=req_json)
        parsed_response = response.json()
        num_max_batch_queries = 0
        for prediction in parsed_response["batch_predictions"]:
            batch_size = prediction["output"]
            if batch_size != self.default_output and int(
                    batch_size) == fixed_batch_size:
                num_max_batch_queries += 1

        self.assertGreaterEqual(num_max_batch_queries,
                                int(total_num_queries * .7))


SHORT_TEST_ORDERING = [
    'test_register_model_correct',
    'test_register_application_correct',
    'test_link_not_registered_model_to_app_fails',
    'test_get_model_links_when_none_exist_returns_empty_list',
    'test_link_registered_model_to_app_succeeds',
    'get_app_info_for_registered_app_returns_info_dictionary',
    'get_app_info_for_nonexistent_app_returns_none',
    'test_set_num_replicas_for_external_model_fails',
    'test_model_version_sets_correctly',
    'test_get_logs_creates_log_files',
    'test_inspect_instance_returns_json_dict',
    'test_model_deploys_successfully',
    'test_set_num_replicas_for_deployed_model_succeeds',
    'test_remove_inactive_containers_succeeds',
    'test_stop_models',
    'test_python_closure_deploys_successfully',
    'test_register_py_endpoint',
]

LONG_TEST_ORDERING = [
    'test_unlinked_app_returns_default_predictions',
    'test_deployed_model_queried_successfully',
    'test_batch_queries_returned_successfully',
    'test_deployed_python_closure_queried_successfully',
    'test_fixed_batch_size_model_processes_specified_query_batch_size_when_saturated'
]

if __name__ == '__main__':
    description = (
        "Runs clipper manager tests. If no arguments are specified, all tests are "
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
