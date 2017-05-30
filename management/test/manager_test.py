import unittest
import sys
import os
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath('%s/../' % cur_dir))
import clipper_manager

class ClipperManagerTestCase(unittest.TestCase):

	@classmethod
	def setUpClass(self):
		self.clipper_inst = clipper_manager.Clipper("localhost")
		self.clipper_inst.start()
		self.model_name = "m1"
		self.model_version1 = 1
		self.model_version2 = 2

	@classmethod
	def tearDownClass(self):
		self.clipper_inst.stop_all()

	def test_external_models_register_correctly(self):
		name = "m1"
		version1 = 1
		tags = ["test"]
		input_type = "doubles"
		result = self.clipper_inst.register_external_model(self.model_name, self.model_version1, tags, input_type)
		self.assertTrue(result)
		registered_model_info = self.clipper_inst.get_model_info(self.model_name, self.model_version1)
		self.assertIsNotNone(registered_model_info)

		version2 = 2
		result = self.clipper_inst.register_external_model(self.model_name, self.model_version2, tags, input_type)
		self.assertTrue(result)
		registered_model_info = self.clipper_inst.get_model_info(self.model_name, self.model_version2)
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
		result = self.clipper_inst.add_container(self.model_name, self.model_version1)
		self.assertFalse(result)

	def test_model_version_sets_correctly(self):
		self.clipper_inst.set_model_version(self.model_name, self.model_version1)
		all_models = self.clipper_inst.get_all_models(verbose=True)
		models_list_contains_correct_version = False
		for model_info in all_models:
			version = model_info["model_version"]
			if version == self.model_version1:
				models_list_contains_correct_version = True
				self.assertTrue(model_info["is_current_version"])

		self.assertTrue(models_list_contains_correct_version)


if __name__ == '__main__':
	suite = unittest.TestSuite()
	test_ordering = ['test_external_models_register_correctly', 'test_application_registers_correctly', 
	'test_add_container_for_external_model_fails', 'test_model_version_sets_correctly']
	for test in test_ordering:
		suite.addTest(ClipperManagerTestCase(test))
	unittest.TextTestRunner(verbosity=2).run(suite)