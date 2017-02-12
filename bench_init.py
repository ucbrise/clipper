from management import clipper_manager
from examples.tutorial import cifar_utils
from sklearn import linear_model as lm
from sklearn.externals import joblib
from fabric.api import *


APP_NAME = "bench"
BASE_DATA_PATH = "bench/data/"
SKLEARN_MODEL_NAME = "bench_sklearn_cifar"
SKLEARN_CONTAINER_NAME = "clipper/sklearn_cifar_container:test"
UID = 0

COLOR_WHITE = '\033[0m'
COLOR_GREEN = '\033[32m'

class BenchSetup():
	def __init__(self, host, cifar_dir_path):
		self.host = host
		self.cifar_dir_path = cifar_dir_path

	def run():
		cifar_test_x, cifar_test_y, cifar_train_x, cifar_train_y = self.get_cifar_data()
		self.train_sklearn_model(cifar_train_x, cifar_train_y, cifar_test_x, cifar_test_y)
		self.deploy_models()


	def get_cifar_data(self):
		train_x, train_y = cifar_utils.filter_data(
			*cifar_utils.load_cifar(self.cifar_dir_path, cifar_filename="cifar_train.data", norm=True))
		test_x, test_y = cifar_utils.filter_data(
			*cifar_utils.load_cifar(self.cifar_dir_path, cifar_filename="cifar_test.data", norm=True))

		return test_x, test_y, train_x, train_y

	def train_sklearn_model(self, train_x, train_y, test_x, test_y):
		model_location = BASE_DATA_PATH + "bench_sk_model.pkl"
		if os.path.isfile(model_location):
			model = joblib.load(model_location)
		else:
			model = lm.LogisticRegression()
			model.fit(train_x, train_y)
			joblib.dump(model, model_location)

		print("Model trained!")
		print("Logistic Regression test score: %f" % model.score(test_x, test_y))

	def deploy_models(self):
		result = 
			self.publish_new_model(SKLEARN_MODEL_NAME, 1, ["cifar", "sklearn"], "doubles", SKLEARN_CONTAINER_NAME, BASE_DATA_PATH)
		print("Model added successfully? {}".format(result))
		if result:
			self.add_container(SKLEARN_MODEL_NAME, 1)
		else:
			raise

	def publish_new_model(
            self,
            name,
            version,
            labels,
            input_type,
            container_name,
            model_data_path):
        url = "http://%s:1338/admin/add_model" % self.host
        req_json = json.dumps({
            "model_name": name,
            "model_version": version,
            "labels": labels,
            "input_type": input_type,
            "container_name": container_name,
            "model_data_path": model_data_path
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code == requests.codes.ok:
            return True
        else:
            warn("Error publishing model: %s" % r.text)
            return False


    def add_container(self, model_name, model_version):
        """Create a new container for an existing model.

        Starts a new container for a model that has already been added to
        Clipper. Note that models are uniquely identified by both name
        and version, so this method will fail if you have not already called
        `Clipper.add_model()` for the specified name and version.

        Parameters
        ----------
        model_name : str
            The name of the model
        model_version : int
            The version of the model
        """
        with hide("warnings", "output", "running"):
            # Look up model info in Redis
            model_key = "{mn}:{mv}".format(mn=model_name, mv=model_version)
            result = local("redis-cli -h {host} -p 6379 -n {db} hgetall {key}".format(
                host=self.host, key=model_key, db=REDIS_MODEL_DB_NUM), capture=True)

            if "nil" in result.stdout:
                # Model not found
                warn(
                    "Trying to add container but model {mn}:{mv} not in "
                    "Redis".format(
                        mn=model_name,
                        mv=model_version))
                return False

            splits = result.stdout.split("\n")
            model_metadata = dict([(splits[i].strip(), splits[i + 1].strip())
                                   for i in range(0, len(splits), 2)])
            image_name = model_metadata["container_name"]
            model_data_path = model_metadata["model_data_path"]
            model_input_type = model_metadata["input_type"]

            # Start container
            add_container_cmd = (
                "docker run -d --network={nw} -v {path}:/model:ro "
                "-e \"CLIPPER_MODEL_NAME={mn}\" -e \"CLIPPER_MODEL_VERSION={mv}\" "
                "-e \"CLIPPER_IP=query_frontend\" -e \"CLIPPER_INPUT_TYPE={mip}\" "
                "{image}".format(
                    path=model_data_path,
                    nw=DOCKER_NW,
                    image=image_name,
                    mn=model_name,
                    mv=model_version,
                    mip=model_input_type))
            result = local(add_container_cmd)
            return result.return_code == 0

if __name__ == '__main__':
	setup = BenchSetup()
	setup.run()

