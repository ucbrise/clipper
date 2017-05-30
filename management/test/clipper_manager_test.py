import unittest
import sys
import os
import json
import time
import requests
from sklearn import svm
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath('%s/../' % cur_dir))
import clipper_manager

class ClipperManagerTestCaseShort(unittest.TestCase):

	@classmethod
	def setUpClass(self):
		self.clipper_inst = clipper_manager.Clipper("localhost")
		self.clipper_inst.start()
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
		tags = ["test"]
		input_type = "doubles"
		result = self.clipper_inst.register_external_model(self.model_name, self.model_version_1, tags, input_type)
		self.assertTrue(result)
		registered_model_info = self.clipper_inst.get_model_info(self.model_name, self.model_version_1)
		self.assertIsNotNone(registered_model_info)

		version2 = 2
		result = self.clipper_inst.register_external_model(self.model_name, self.model_version_2, tags, input_type)
		self.assertTrue(result)
		registered_model_info = self.clipper_inst.get_model_info(self.model_name, self.model_version_2)
		self.assertIsNotNone(registered_model_info)

	def test_application_registers_correctly(self):
		app_name = "app1"
		input_type = "doubles"
		default_output = "DEFAULT"
		slo_micros = 30000
		self.clipper_inst.register_application(app_name, self.model_name, input_type, default_output, slo_micros)
		registered_applications = self.clipper_inst.get_all_apps()
		self.assertGreaterEqual(len(registered_applications), 1)
		self.assertTrue(app_name in registered_applications)

	def test_add_container_for_external_model_fails(self):
		result = self.clipper_inst.add_container(self.model_name, self.model_version_1)
		self.assertFalse(result)

	def test_model_version_sets_correctly(self):
		self.clipper_inst.set_model_version(self.model_name, self.model_version_1)
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
		labels = ["test"]
		input_type = "doubles"
		result = self.clipper_inst.deploy_model(self.deploy_model_name, self.deploy_model_version, model_data, container_name, labels, input_type)
		self.assertTrue(result)
		model_info = self.clipper_inst.get_model_info(self.deploy_model_name, self.deploy_model_version)
		self.assertIsNotNone(model_info)
		running_containers_output = self.clipper_inst._execute_standard("docker ps -q --filter \"ancestor=clipper/noop-container\"")
		self.assertIsNotNone(running_containers_output)
		self.assertGreaterEqual(len(running_containers_output), 1)

	def test_add_container_for_deployed_model_succeeds(self):
		result = self.clipper_inst.add_container(self.deploy_model_name, self.deploy_model_version)
		self.assertTrue(result)
		running_containers_output = self.clipper_inst._execute_standard("docker ps -q --filter \"ancestor=clipper/noop-container\"")
		self.assertIsNotNone(running_containers_output)
		split_output = running_containers_output.split("\n")
		self.assertGreaterEqual(len(split_output), 2)

	def test_predict_function_deploys_successfully(self):
		model_name = "m2"
		model_version = 1
		predict_func = lambda inputs : ["0" for x in inputs]
		labels = ["test"]
		input_type = "doubles"
		result = self.clipper_inst.deploy_predict_function(model_name, model_version, predict_func, labels, input_type)
		self.assertTrue(result)
		model_info = self.clipper_inst.get_model_info(model_name, model_version)
		self.assertIsNotNone(model_info)
		running_containers_output = self.clipper_inst._execute_standard("docker ps -q --filter \"ancestor=clipper/python-container\"")
		self.assertIsNotNone(running_containers_output)
		self.assertGreaterEqual(len(running_containers_output), 1)


class ClipperManagerTestCaseLong(unittest.TestCase):

	@classmethod
	def setUpClass(self):
		self.clipper_inst = clipper_manager.Clipper("localhost")
		self.clipper_inst.start()
		self.app_name_1 = "app1"
		self.app_name_2 = "app2"
		self.model_name_1 = "m1"
		self.model_name_2 = "m2"
		self.input_type = "doubles"
		self.default_output = "DEFAULT"
		self.latency_slo_micros = 30000
		self.clipper_inst.register_application(self.app_name_1, self.model_name_1, self.input_type, self.default_output, self.latency_slo_micros)
		self.clipper_inst.register_application(self.app_name_2, self.model_name_2, self.input_type, self.default_output, self.latency_slo_micros)

	@classmethod
	def tearDownClass(self):
		self.clipper_inst.stop_all()

	def test_deployed_model_queried_successfully(self):
		model_version = 1
		# Initialize a support vector classifier 
		# that will be deployed to a no-op container
		model_data = svm.SVC()
		container_name = "clipper/noop-container"
		labels = ["test"]
		result = self.clipper_inst.deploy_model(self.model_name_2, model_version, model_data, container_name, labels, self.input_type)
		self.assertTrue(result)

		time.sleep(30)

		url = "http://localhost:1337/{}/predict".format(self.app_name_2)
		test_input = [99.3, 18.9, 67.2, 34.2]
		req_json = json.dumps({'uid': 0, 'input': test_input})
		headers = {'Content-type': 'application/json'}
		response = requests.post(url, headers=headers, data=req_json)
		parsed_response = json.loads(response.text)
		self.assertNotEqual(parsed_response["output"], self.default_output)

	def test_deployed_predict_function_queried_successfully(self):
		model_version = 1
		predict_func = lambda inputs : [str(len(x)) for x in inputs]
		labels = ["test"]
		input_type = "doubles"
		result = self.clipper_inst.deploy_predict_function(self.model_name_1, model_version, predict_func, labels, input_type)
		self.assertTrue(result)

		time.sleep(60)

		received_non_default_prediction = False
		for i in range(0, 40):
			url = "http://localhost:1337/{}/predict".format(self.app_name_1)
			test_input = [101.1, 99.5, 107.2]
			req_json = json.dumps({'uid': 0, 'input': test_input})
			headers = {'Content-type': 'application/json'}
			response = requests.post(url, headers=headers, data=req_json)
			parsed_response = json.loads(response.text)
			output = parsed_response["output"]
			if output == self.default_output:
				time.sleep(20)
			else:
				received_non_default_prediction = True
				self.assertEqual(int(output), len(test_input))
				break

		self.assertTrue(received_non_default_prediction)


SHORT_ARGS = ['s', 'short']
LONG_ARGS = ['l', 'long']

SHORT_TEST_ORDERING = ['test_external_models_register_correctly', 'test_application_registers_correctly', 
		'test_add_container_for_external_model_fails', 'test_model_version_sets_correctly', 'test_get_logs_creates_log_files',
		'test_inspect_instance_returns_json_dict', 'test_model_deploys_successfully', 'test_add_container_for_deployed_model_succeeds',
		'test_predict_function_deploys_successfully']

LONG_TEST_ORDERING = ['test_deployed_model_queried_successfully', 'test_deployed_predict_function_queried_successfully']

if __name__ == '__main__':
	run_short = True
	run_long = True
	if len(sys.argv) > 1:
		if sys.argv[1] in SHORT_ARGS:
			run_long = False
		elif sys.argv[1] in LONG_ARGS:
			run_short = False
		else:
			print("Correct parameter values are either 's'/'short' or 'l'/'long' indicating which subset of tests should be executed!")
			raise

	suite = unittest.TestSuite()

	if run_short:
		for test in SHORT_TEST_ORDERING:
			suite.addTest(ClipperManagerTestCaseShort(test))

	if run_long:
		for test in LONG_TEST_ORDERING:
			suite.addTest(ClipperManagerTestCaseLong(test))

	result = unittest.TextTestRunner(verbosity=2).run(suite)
	sys.exit(not result.wasSuccessful())