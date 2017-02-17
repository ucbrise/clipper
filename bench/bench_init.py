import sys
import os
import json
import requests
sys.path.append("..")

from management import clipper_manager
from examples.tutorial import cifar_utils
from sklearn import linear_model as lm
from sklearn.externals import joblib
from fabric.api import *


APP_NAME = "bench"
BASE_DATA_PATH = "data/"
SKLEARN_MODEL_FILE = "bench_sk_model.pkl"
SKLEARN_MODEL_NAME = "bench_sklearn_cifar"

COLOR_WHITE = '\033[0m'
COLOR_GREEN = '\033[32m'

class BenchSetup():
	def __init__(self, host, cifar_dir_path):
		self.host = host
		self.cifar_dir_path = cifar_dir_path

	def print_green(self, text):
		print(COLOR_GREEN)
		print(text)
		print(COLOR_WHITE)

	def run(self):
		self.print_green("Loading Sklearn Model...")
		self.train_sklearn_model()


	def get_cifar_data(self):
		train_x, train_y = cifar_utils.filter_data(
			*cifar_utils.load_cifar(self.cifar_dir_path, cifar_filename="cifar_train.data", norm=False))
		test_x, test_y = cifar_utils.filter_data(
			*cifar_utils.load_cifar(self.cifar_dir_path, cifar_filename="cifar_test.data", norm=False))

		return test_x, test_y, train_x, train_y

	def train_sklearn_model(self):
		model_location = BASE_DATA_PATH + SKLEARN_MODEL_FILE
		if os.path.isfile(model_location):
			model = joblib.load(model_location)
			print("Found and loaded model!")
		else:
			print("Loading CIFAR data...")
			test_x, test_y, train_x, train_y = self.get_cifar_data()
			print("Training model...")
			model = lm.LogisticRegression()
			model.fit(train_x, train_y)
			joblib.dump(model, model_location)
			print("Model trained!")
			print("Logistic Regression test score: %f" % model.score(test_x, test_y))

if __name__ == '__main__':
	setup = BenchSetup("localhost", "../examples/cifar_demo")
	setup.run()

