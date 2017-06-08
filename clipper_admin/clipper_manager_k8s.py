"""Clipper Management Utilities"""

from __future__ import print_function, with_statement, absolute_import
import docker
from fabric.api import *
from fabric.contrib.files import append
import os
import requests
import json
import yaml
import subprocess32 as subprocess
import shutil
from sklearn import base
from sklearn.externals import joblib
from cStringIO import StringIO
from .cloudpickle import CloudPickler
from .clipper_k8s import ClipperK8s
import logging
import time
import re

__all__ = ['Clipper']

cur_dir = os.path.dirname(os.path.abspath(__file__))

MODEL_REPO = "/tmp/clipper-models"
DOCKER_NW = "clipper_nw"

CONTAINER_CONDA_PLATFORM = 'linux-64'

REDIS_STATE_DB_NUM = 1
REDIS_MODEL_DB_NUM = 2
REDIS_CONTAINER_DB_NUM = 3
REDIS_RESOURCE_DB_NUM = 4
REDIS_APPLICATION_DB_NUM = 5

DEFAULT_REDIS_PORT = 6379
CLIPPER_QUERY_PORT = 1337
CLIPPER_MANAGEMENT_PORT = 1338
CLIPPER_RPC_PORT = 7000

CLIPPER_LOGS_PATH = "/tmp/clipper-logs"

CLIPPER_DOCKER_LABEL = "ai.clipper.container.label"
CLIPPER_MODEL_CONTAINER_LABEL = "ai.clipper.model_container.label"

DEFAULT_LABEL = ["DEFAULT"]

aws_cli_config = """
[default]
region = us-east-1
aws_access_key_id = {access_key}
aws_secret_access_key = {secret_key}
"""

LOCAL_HOST_NAMES = ["local", "localhost", "127.0.0.1"]

EXTERNALLY_MANAGED_MODEL = "EXTERNAL"


class ClipperManagerException(Exception):
    pass


class Clipper:
    """
    Connection to a Clipper instance for administrative purposes.

    Sets up the machine for running Clipper. This includes verifying
    SSH credentials and initializing Docker.

    Docker and docker-compose must already by installed on the machine
    before connecting to a machine.

    Parameters
    ----------
    host : str
        The hostname of the machine to start Clipper on. The machine
        should allow passwordless SSH access.
    user : str, optional
        The SSH username. This field must be specified if `host` is not local.
    key_path : str, optional.
        The path to the SSH private key. This field must be specified if `host` is not local.
    sudo : bool, optional.
        Specifies level of execution for docker commands (sudo if true, standard if false).
    ssh_port : int, optional
        The SSH port to use. Default is port 22.
    check_for_docker : bool, optional
        If True, checks that Docker is running on the host machine. Default is True.
    restart_containers : bool, optional
        If true, containers will restart on failure. If false, containers
        will not restart automatically.

    """

    def __init__(self,
                 host,
                 user=None,
                 key_path=None,
                 sudo=False,
                 ssh_port=22,
                 check_for_docker=True,
                 restart_containers=True):
        # TODO: support deploying redis host off-cluster by taking redis_ip as constructor param to ClipperK8s
        logging.basicConfig(level=logging.INFO)
        self.clipper_k8s = ClipperK8s()
        self.sudo = sudo
        self.host = host
        self.host_string = self.host

    def _host_is_local(self):
        return True

    def _execute_root(self, *args, **kwargs):
        if not self.sudo:
            return self._execute_standard(*args, **kwargs)
        elif self._host_is_local():
            return self._execute_local(True, *args, **kwargs)
        else:
            return sudo(*args, **kwargs)

    def _execute_standard(self, *args, **kwargs):
        if self._host_is_local():
            return self._execute_local(False, *args, **kwargs)
        else:
            return run(*args, **kwargs)

    def _execute_local(self, as_root, *args, **kwargs):
        if self.sudo and as_root:
            root_args = list(args)
            root_args[0] = "sudo %s" % root_args[0]
            args = tuple(root_args)
        # local is not currently capable of simultaneously logging.infoing and
        # capturing output, as run/sudo do. The capture kwarg allows you to
        # switch between logging.infoing and capturing as necessary, and defaults to
        # False. In this case, we need to capture the output and return it.
        if "capture" not in kwargs:
            kwargs["capture"] = True
        # fabric.local() does not accept the "warn_only"
        # key word argument, so we must remove it before
        # calling
        if "warn_only" in kwargs:
            del kwargs["warn_only"]
            # Forces execution to continue in the face of an error,
            # just like warn_only=True
            with warn_only():
                result = local(*args, **kwargs)
        else:
            result = local(*args, **kwargs)
        return result

    def _execute_append(self, filename, text, **kwargs):
        if self._host_is_local():
            file = open(filename, "a+")
            # As with fabric.append(), we should only
            # append the text if it is not already
            # present within the file
            if text not in file.read():
                file.write(text)
            file.close()
        else:
            append(filename, text, **kwargs)

    def _execute_put(self, local_path, remote_path, *args, **kwargs):
        if self._host_is_local():
            # We should only copy data if the paths are different
            if local_path != remote_path:
                if os.path.isdir(local_path):
                    remote_path = os.path.join(remote_path,
                                               os.path.basename(local_path))
                    # if remote_path exists, delete it because shutil.copytree requires
                    # that the dst path doesn't exist
                    if os.path.exists(remote_path):
                        shutil.rmtree(remote_path)
                    shutil.copytree(local_path, remote_path)
                else:
                    shutil.copy2(local_path, remote_path)
        else:
            put(
                local_path=local_path,
                remote_path=remote_path,
                *args,
                **kwargs)

    def start(self):
        """Start a Clipper instance.

        """
        self.clipper_k8s.start()
        logging.info("Clipper is running")

    def register_application(self, name, model, input_type, default_output,
                             slo_micros):
        """Register a new Clipper application.

        Parameters
        ----------
        name : str
            The name of the application.
        model : str
            The name of the model this application will query.
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        default_output : string
            The default prediction to use if the model does not return a prediction
            by the end of the latency objective.
        slo_micros : int
            The query latency objective for the application in microseconds.
            This is the processing latency between Clipper receiving a request
            and sending a response. It does not account for network latencies
            before a request is received or after a response is sent.

            If Clipper cannot process a query within the latency objective,
            the default output is returned. Therefore, it is recommended that
            the objective not be set aggressively low unless absolutely necessary.
            40000 (40ms) is a good starting value, but the optimal latency objective
            will vary depending on the application.
        """
        url = "http://%s:%d/admin/add_app" % (self.host,
                                              CLIPPER_MANAGEMENT_PORT)
        req_json = json.dumps({
            "name": name,
            "candidate_model_names": [model],
            "input_type": input_type,
            "default_output": default_output,
            "latency_slo_micros": slo_micros
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        logging.info(r.text)

    def get_all_apps(self, verbose=False):
        """Gets information about all applications registered with Clipper.

        Parameters
        ----------
        verbose : bool
            If set to False, the returned list contains the apps' names.
            If set to True, the list contains application info dictionaries.
            These dictionaries have the same attribute name-value pairs that were
            provided to `register_application`.

        Returns
        -------
        list
            Returns a list of information about all apps registered to Clipper.
            If no apps are registered with Clipper, an empty list is returned.
        """
        url = "http://%s:1338/admin/get_all_applications" % self.host
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            logging.warn(r.text)
            return None

    def get_app_info(self, name):
        """Gets detailed information about a registered application.

        Parameters
        ----------
        name : str
            The name of the application to look up

        Returns
        -------
        dict
            Returns a dictionary with the specified application's info. This
            will contain the attribute name-value pairs that were provided to
            `register_application`. If no application with name `name` is
            registered with Clipper, None is returned.
        """
        url = "http://%s:1338/admin/get_application" % self.host
        req_json = json.dumps({"name": name})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                return None
            return app_info
        else:
            logging.warn(r.text)
            return None

    def deploy_model(self,
                     name,
                     version,
                     model_data,
                     container_name,
                     input_type,
                     labels=DEFAULT_LABEL,
                     num_containers=1):
        """Registers a model with Clipper and deploys instances of it in containers.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        model_data : str or BaseEstimator
            The trained model to add to Clipper. This can either be a
            Scikit-Learn trained model object (an instance of BaseEstimator),
            or a path to a serialized model. Note that many model serialization
            formats split the model across multiple files (e.g. definition file
            and weights file or files). If this is the case, `model_data` must be a path
            to the root of a directory tree containing ALL the needed files.
            Depending on the model serialization library you use, this may or may not
            be the path you provided to the serialize method call.
        container_name : str
            The Docker container image to use to run this model container.
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        labels : list of str, optional
            A list of strings annotating the model
        num_containers : int, optional
            The number of replicas of the model to create. More replicas can be
            created later as well. Defaults to 1.
        """
        with hide("warnings", "output", "running"):
            if isinstance(model_data, base.BaseEstimator):
                fname = name.replace("/", "_")
                model_data_path = "/tmp/%s" % fname
                pkl_path = '%s/%s.pkl' % (model_data_path, fname)
                try:
                    os.mkdir(model_data_path)
                except OSError:
                    pass
                joblib.dump(model_data, pkl_path)
            elif isinstance(model_data, str):
                # assume that model_data is a path to the serialized model
                model_data_path = model_data
            else:
                warn("%s is invalid model format" % str(type(model_data)))
                return False

            vol = "{model_repo}/{name}/{version}".format(
                model_repo=MODEL_REPO, name=name, version=version)

            # prepare docker image build dir
            with open(model_data_path + '/Dockerfile', 'w') as f:
                f.write("FROM {container_name}\nCOPY . /model/.\n".format(
                    container_name=container_name))

            # build, tag, and push docker image to registry
            # NOTE: DOCKER_API_VERSION (set by `minikube docker-env`) must be same version as docker registry server
            repo = '{docker_registry}/{name}:{version}'.format(
                docker_registry='localhost:5000',  # TODO: make configurable
                name=name,
                version=version)
            docker_client = docker.from_env(
                version=os.environ["DOCKER_API_VERSION"])
            logging.info(
                "Building model Docker image at {}".format(model_data_path))
            docker_client.images.build(path=model_data_path, tag=repo)
            logging.info("Pushing model Docker image to {}".format(repo))
            docker_client.images.push(repository=repo)

            # TODO: call this in `add_container` once `repo` is available from redis
            logging.info("Creating model deployment on k8s")
            self.clipper_k8s.deploy_model(name, version, repo)

            # TODO: replace `model_data_path` in `_publish_new_model` with the docker repo
            # publish model to Clipper and verify success before copying model
            # parameters to Clipper and starting containers
            logging.info("Publishing model to Clipper query manager")
            self._publish_new_model(name, version, labels, input_type,
                                    container_name, repo)

            logging.info("Done deploying!")

            # aggregate results of starting all containers
            # return all([
            #     self.add_container(name, version)
            #     for r in range(num_containers)
            # ])

    def register_external_model(self,
                                name,
                                version,
                                input_type,
                                labels=DEFAULT_LABEL):
        """Registers a model with Clipper without deploying it in any containers.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        labels : list of str, optional
            A list of strings annotating the model.
        """
        # TODO: this could be implemented by taking a docker repo and deploying a container with it
        return self._publish_new_model(name, version, labels, input_type,
                                       EXTERNALLY_MANAGED_MODEL,
                                       EXTERNALLY_MANAGED_MODEL)

    def _save_python_function(self, name, predict_function):
        relative_base_serializations_dir = "predict_serializations"
        predict_fname = "predict_func.pkl"
        environment_fname = "environment.yml"
        conda_dep_fname = "conda_dependencies.txt"
        pip_dep_fname = "pip_dependencies.txt"

        # Serialize function
        s = StringIO()
        c = CloudPickler(s, 2)
        c.dump(predict_function)
        serialized_prediction_function = s.getvalue()

        # Set up serialization directory
        serialization_dir = os.path.join(
            '/tmp', relative_base_serializations_dir, name)
        if not os.path.exists(serialization_dir):
            os.makedirs(serialization_dir)

        # Export Anaconda environment
        environment_file_abs_path = os.path.join(serialization_dir,
                                                 environment_fname)

        conda_env_exported = self._export_conda_env(environment_file_abs_path)

        if conda_env_exported:
            logging.info("Anaconda environment found. Verifying packages.")

            # Confirm that packages installed through conda are solvable
            # Write out conda and pip dependency files to be supplied to container
            if not (self._check_and_write_dependencies(
                    environment_file_abs_path, serialization_dir,
                    conda_dep_fname, pip_dep_fname)):
                return False

            logging.info("Supplied environment details")
        else:
            logging.info(
                "Warning: Anaconda environment was either not found or exporting the environment "
                "failed. Your function will still be serialized and deployed, but may fail due to "
                "missing dependencies. In this case, please re-run inside an Anaconda environment. "
                "See http://clipper.ai/documentation/python_model_deployment/ for more information."
            )

        # Write out function serialization
        func_file_path = os.path.join(serialization_dir, predict_fname)
        with open(func_file_path, "w") as serialized_function_file:
            serialized_function_file.write(serialized_prediction_function)
        logging.info("Serialized and supplied predict function")
        return serialization_dir

    def deploy_pyspark_model(self,
                             name,
                             version,
                             predict_function,
                             pyspark_model,
                             sc,
                             input_type,
                             labels=DEFAULT_LABEL,
                             num_containers=1):
        """Deploy a Spark MLLib model to Clipper.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        predict_function : function
            A function that takes three arguments, a SparkContext, the ``model`` parameter and
            a list of inputs of the type specified by the ``input_type`` argument.
            Any state associated with the function other than the Spark model should
            be captured via closure capture. Note that the function must not capture
            the SparkContext or the model implicitly, as these objects are not pickleable
            and therefore will prevent the ``predict_function`` from being serialized.
        pyspark_model : pyspark.mllib.util.Saveable
            An object that mixes in the pyspark Saveable mixin. Generally this
            is either an mllib model or transformer. This model will be loaded
            into the Clipper model container and provided as an argument to the
            predict function each time it is called.
        sc : SparkContext
            The SparkContext associated with the model. This is needed
            to save the model for pyspark.mllib models.
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        labels : list of str, optional
            A set of strings annotating the model
        num_containers : int, optional
            The number of replicas of the model to create. More replicas can be
            created later as well. Defaults to 1.

        Returns
        -------
        bool
            True if the model was successfully deployed. False otherwise.
        """

        model_class = re.search("pyspark.*'",
                                str(type(pyspark_model))).group(0).strip("'")
        if model_class is None:
            raise ClipperManagerException(
                "pyspark_model argument was not a pyspark object")

        # save predict function
        serialization_dir = self._save_python_function(name, predict_function)
        # save Spark model
        spark_model_save_loc = os.path.join(serialization_dir,
                                            "pyspark_model_data")
        try:
            # we only import pyspark here so that if the caller of the library does
            # not want to use this function, clipper_manager does not have a dependency
            # on pyspark
            import pyspark
            if isinstance(pyspark_model, pyspark.ml.pipeline.PipelineModel):
                pyspark_model.save(spark_model_save_loc)
            else:
                pyspark_model.save(sc, spark_model_save_loc)
        except Exception as e:
            logging.warn("Error saving spark model: %s" % e)
            raise e

        pyspark_container = "clipper/pyspark-container"

        # extract the pyspark class name. This will be something like
        # pyspark.mllib.classification.LogisticRegressionModel
        with open(os.path.join(serialization_dir, "metadata.json"),
                  "w") as metadata_file:
            json.dump({"model_class": model_class}, metadata_file)

        logging.info("Spark model saved")

        # Deploy model
        deploy_result = self.deploy_model(name, version, serialization_dir,
                                          pyspark_container, input_type,
                                          labels, num_containers)

        # Remove temp files
        shutil.rmtree(serialization_dir)

        return deploy_result

    def deploy_predict_function(self,
                                name,
                                version,
                                predict_function,
                                input_type,
                                labels=DEFAULT_LABEL,
                                num_containers=1):
        """Deploy an arbitrary Python function to Clipper.

        The function should take a list of inputs of the type specified by `input_type` and
        return a Python or numpy array of predictions as strings. All dependencies for the function
        must be installed with Anaconda or Pip and this function must be called from within an Anaconda
        environment.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        predict_function : function
            The prediction function. Any state associated with the function should be
            captured via closure capture.
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        labels : list of str, optional
            A list of strings annotating the model
        num_containers : int, optional
            The number of replicas of the model to create. More replicas can be
            created later as well. Defaults to 1.

        Returns
        -------
        bool
            True if the model was successfully deployed. False otherwise.

        Example
        -------
        Define a feature function ``center()`` and train a model on the featurized input::

            def center(xs):
                means = np.mean(xs, axis=0)
                return xs - means

            centered_xs = center(xs)
            model = sklearn.linear_model.LogisticRegression()
            model.fit(centered_xs, ys)

            def centered_predict(inputs):
                centered_inputs = center(inputs)
                return model.predict(centered_inputs)

            clipper.deploy_predict_function(
                "example_model",
                1,
                centered_predict,
                "doubles",
                num_containers=1)
        """

        default_python_container = "clipper/python-container"
        serialization_dir = self._save_python_function(name, predict_function)

        # Deploy function
        deploy_result = self.deploy_model(name, version, serialization_dir,
                                          default_python_container, input_type,
                                          labels, num_containers)
        # Remove temp files
        shutil.rmtree(serialization_dir)

        return deploy_result

    def get_all_models(self, verbose=False):
        """Gets information about all models registered with Clipper.

        Parameters
        ----------
        verbose : bool
            If set to False, the returned list contains the models' names.
            If set to True, the list contains model info dictionaries.

        Returns
        -------
        list
            Returns a list of information about all apps registered to Clipper.
            If no models are registered with Clipper, an empty list is returned.
        """
        url = "http://%s:1338/admin/get_all_models" % self.host
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            logging.info(r.text)
            return None

    def get_model_info(self, model_name, model_version):
        """Gets detailed information about a registered model.

        Parameters
        ----------
        model_name : str
            The name of the model to look up
        model_version : int
            The version of the model to look up

        Returns
        -------
        dict
            Returns a dictionary with the specified model's info.
            If no model with name `model_name@model_version` is
            registered with Clipper, None is returned.
        """
        url = "http://%s:1338/admin/get_model" % self.host
        req_json = json.dumps({
            "model_name": model_name,
            "model_version": model_version
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                return None
            return app_info
        else:
            logging.info(r.text)
            return None

    def get_all_containers(self, verbose=False):
        """Gets information about all containers registered with Clipper.

        Parameters
        ----------
        verbose : bool
            If set to False, the returned list contains the apps' names.
            If set to True, the list contains container info dictionaries.

        Returns
        -------
        list
            Returns a list of information about all apps registered to Clipper.
            If no containerss are registered with Clipper, an empty list is returned.
        """
        url = "http://%s:1338/admin/get_all_containers" % self.host
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            logging.info(r.text)
            return None

    def get_container_info(self, model_name, model_version, replica_id):
        """Gets detailed information about a registered container.

        Parameters
        ----------
        model_name : str
            The name of the container to look up
        model_version : int
            The version of the container to look up
        replica_id : int
            The container replica to look up

        Returns
        -------
        dict
            A dictionary with the specified container's info.
            If no corresponding container is registered with Clipper, None is returned.
        """
        url = "http://%s:1338/admin/get_container" % self.host
        req_json = json.dumps({
            "model_name": model_name,
            "model_version": model_version,
            "replica_id": replica_id,
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                return None
            return app_info
        else:
            logging.info(r.text)
            return None

    def _inspect_selection_policy(self, app_name, uid):
        # NOTE: This method is private (it's still functional, but it won't be documented)
        # until Clipper supports different selection policies
        """Fetches a human-readable string with the current selection policy state.

        Parameters
        ----------
        app_name : str
            The application whose policy state should be inspected.
        uid : int
            The user whose policy state should be inspected. The convention
            in Clipper is to use 0 as the default user ID, but this may be
            application specific.

        Returns
        -------
        str
            The string describing the selection state. Note that if the
            policy state was not found, this string may contain an error
            message from Clipper describing the problem.
        """

        url = "http://%s:%d/admin/get_state" % (self.host,
                                                CLIPPER_MANAGEMENT_PORT)
        req_json = json.dumps({
            "app_name": app_name,
            "uid": uid,
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        return r.text

    def _export_conda_env(self, environment_file_abs_path):
        """Returns true if attempt to export the current conda environment is successful

        Parameters
        ----------
        environment_file_abs_path : str
            The desired absolute path for the exported conda environment file
        """

        process = subprocess.Popen(
            "PIP_FORMAT=legacy conda env export >> {environment_file_abs_path}".
            format(environment_file_abs_path=environment_file_abs_path),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=True)
        process.wait()
        return process.returncode == 0

    def _check_and_write_dependencies(self, environment_path, directory,
                                      conda_dep_fname, pip_dep_fname):
        """Returns true if the provided conda environment is compatible with the container os.

        If packages listed in specified conda environment file have conflicting dependencies,
        this function will warn the user and return False.

        If there are no conflicting package dependencies, existence of the packages in the
        container conda channel is tested. The user is warned about any missing packages.
        All existing conda packages are written out to `conda_dep_fname` and pip packages
        to `pip_dep_fname` in the given `directory`. This function then returns True.

        Parameters
        ----------
        environment_path : str
            The path to the input conda environment file
        directory : str
            The path to the diretory containing the environment file
        conda_dep_fname : str
            The name of the output conda dependency file
        pip_dep_fname : str
            The name of the output pip dependency file

        Returns
        -------
        bool
            Returns True if the packages specified in `environment_fname` are compatible with conda
            on the container os. Otherwise returns False.
        """
        if "CONDA_PREFIX" not in os.environ:
            logging.info("No Anaconda environment found")
            return False

        root_prefix = os.environ["CONDA_PREFIX"].split("envs")[0]
        py_path = os.path.join(root_prefix, "bin", "python")
        process = subprocess.Popen(
            "{py_path} {cur_dir}/check_and_write_deps.py {environment_path} {directory} {platform} {conda_dep_fname} {pip_dep_fname}".
            format(
                py_path=py_path,
                cur_dir=cur_dir,
                environment_path=environment_path,
                directory=directory,
                platform=CONTAINER_CONDA_PLATFORM,
                conda_dep_fname=conda_dep_fname,
                pip_dep_fname=pip_dep_fname),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=True)
        out, err = process.communicate()
        logging.info(out)
        logging.info(err)
        return process.returncode == 0

    def add_container(self, model_name, model_version):
        """Create a new container for an existing model.

        Starts a new container for a model that has already been added to
        Clipper. Note that models are uniquely identified by both name
        and version, so this method will fail if you have not already called
        `Clipper.deploy_model()` for the specified name and version.

        Parameters
        ----------
        model_name : str
            The name of the model
        model_version : int
            The version of the model

        Returns
        ----------
        bool
            True if the container was added successfully and False
            if the container could not be added.
        """
        # TODO: this must abstract containers deployed on k8s vs those running on local docker, see ContainerManager
        with hide("warnings", "output", "running"):
            # Look up model info in Redis
            model_key = "{mn}:{mv}".format(mn=model_name, mv=model_version)
            result = local(
                "redis-cli -h {host} -p {redis_port} -n {db} hgetall {key}".
                format(
                    host=self.host,
                    redis_port=DEFAULT_REDIS_PORT,
                    key=model_key,
                    db=REDIS_MODEL_DB_NUM),
                capture=True)
            logging.info(result)

            if "empty list or set" in result.stdout:
                # Model not found
                warn("Trying to add container but model {mn}:{mv} not in "
                     "Redis".format(mn=model_name, mv=model_version))
                return False

            splits = result.stdout.split("\n")
            model_metadata = dict([(splits[i].strip(), splits[i + 1].strip())
                                   for i in range(0, len(splits), 2)])
            image_name = model_metadata["container_name"]
            model_data_path = model_metadata["model_data_path"]
            model_input_type = model_metadata["input_type"]
            restart_policy = 'always' if self.restart_containers else 'no'

            if image_name != EXTERNALLY_MANAGED_MODEL:
                # Start container
                add_container_cmd = (
                    "docker run -d --network={nw} --restart={restart_policy} -v {path}:/model:ro "
                    "-e \"CLIPPER_MODEL_NAME={mn}\" -e \"CLIPPER_MODEL_VERSION={mv}\" "
                    "-e \"CLIPPER_IP=query_frontend\" -e \"CLIPPER_INPUT_TYPE={mip}\" -l \"{clipper_label}\" -l \"{mv_label}\" "
                    "{image}".format(
                        path=model_data_path,
                        nw=DOCKER_NW,
                        image=image_name,
                        mn=model_name,
                        mv=model_version,
                        mip=model_input_type,
                        clipper_label=CLIPPER_DOCKER_LABEL,
                        mv_label="%s=%s:%d" % (CLIPPER_MODEL_CONTAINER_LABEL,
                                               model_name, model_version),
                        restart_policy=restart_policy))
                result = self._execute_root(add_container_cmd)
                return result.return_code == 0
            else:
                logging.info(
                    "Cannot start containers for externally managed model %s" %
                    model_name)
                return False

    def get_clipper_logs(self):
        """Copies the logs from all Docker containers running on the host machine
        that have been tagged with the Clipper label (ai.clipper.container.label) into
        the local filesystem.

        Returns
        -------
        list(str)
            Returns a list of local filenames containing the Docker container log snapshots.
        """
        container_ids = self._get_clipper_container_ids()
        cur_time_logs_path = os.path.join(CLIPPER_LOGS_PATH,
                                          time.strftime("%Y%m%d-%H%M%S"))
        if not os.path.exists(cur_time_logs_path):
            os.makedirs(cur_time_logs_path)
        log_file_names = []
        for container in container_ids:
            output = self._execute_root(
                "docker logs {container}".format(container=container))
            cur_log_fname = os.path.join(cur_time_logs_path,
                                         "%s-container.log" % container)
            with open(cur_log_fname, "w") as f:
                f.write(output)
            log_file_names.append(cur_log_fname)
        return log_file_names

    def _get_clipper_container_ids(self):
        """
        Gets the container IDs of all containers labeled with the clipper label
        """
        containers = self._execute_root(
            "docker ps -aq --filter label={clipper_label}".format(
                clipper_label=CLIPPER_DOCKER_LABEL))
        ids = [l.strip() for l in containers.split("\n")]
        logging.info("Clipper container IDS found: %s" % str(ids))
        return ids

    def inspect_instance(self):
        """Fetches metrics from the running Clipper instance.

        Returns
        -------
        str
            The JSON string containing the current set of metrics
            for this instance. On error, the string will be an error message
            (not JSON formatted).
        """
        url = "http://%s:%d/metrics" % (self.host, CLIPPER_QUERY_PORT)
        r = requests.get(url)
        try:
            s = r.json()
        except TypeError:
            s = r.text
        return s

    def set_model_version(self, model_name, model_version, num_containers=0):
        """Changes the current model version to `model_version`.

        This method can be used to do model rollback and rollforward to
        any previously deployed version of the model. Note that model
        versions automatically get updated when `deploy_model()` is
        called, so there is no need to manually update the version as well.

        Parameters
        ----------
        model_name : str
            The name of the model
        model_version : int
            The version of the model. Note that `model_version`
            must be a model version that has already been deployed.
        num_containers : int
            The number of new containers to start with the newly
            selected model version.

        """
        # TODO: update to use k8s API
        url = "http://%s:%d/admin/set_model_version" % (
            self.host, CLIPPER_MANAGEMENT_PORT)
        req_json = json.dumps({
            "model_name": model_name,
            "model_version": model_version
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        logging.info(r.text)
        for r in range(num_containers):
            self.add_container(model_name, model_version)

    def remove_inactive_containers(self, model_name):
        """Removes all containers serving stale versions of the specified model.

        Parameters
        ----------
        model_name : str
            The name of the model whose old containers you want to clean.

        """
        # Get all Docker containers tagged as model containers
        num_containers_removed = 0
        with hide("output", "warnings", "running"):
            container_ids = self._get_clipper_container_ids()
            if len(container_ids) > 0:
                for container in container_ids:
                    # returns a string formatted as "<model_name>:<model_version>"
                    container_model_name_and_version = self._execute_root(
                        "docker inspect --format \"{{ index .Config.Labels \\\"%s\\\"}}\" %s"
                        % (CLIPPER_MODEL_CONTAINER_LABEL, container))
                    splits = container_model_name_and_version.split(":")
                    container_model_name = splits[0]
                    container_model_version = int(splits[1])
                    if container_model_name == model_name:
                        # check if container_model_version is the currently deployed version
                        model_info = self.get_model_info(
                            container_model_name, container_model_version)
                        if model_info == None or not model_info["is_current_version"]:
                            self._execute_root("docker stop {container}".
                                               format(container=container))
                            self._execute_root("docker rm {container}".format(
                                container=container))
                            num_containers_removed += 1
        logging.info("Removed %d inactive containers for model %s" %
                     (num_containers_removed, model_name))
        return num_containers_removed

    def stop_all(self):
        """Stops and removes all Clipper model deployments and Clipper resources."""
        self.clipper_k8s.stop_all_model_deployments()
        self.clipper_k8s.stop_clipper_resources()

    # TODO: provide registry image for k8s service instead of container_name and model_data_path
    def _publish_new_model(self, name, version, labels, input_type,
                           container_name, model_data_path):
        url = "http://%s:%d/admin/add_model" % (self.host,
                                                CLIPPER_MANAGEMENT_PORT)
        req_json = json.dumps({
            "model_name": name,
            "model_version": str(version),
            "labels": labels,
            "input_type": input_type,
            "container_name": container_name,
            "model_data_path": model_data_path
        })
        headers = {'Content-type': 'application/json'}
        logging.info(req_json)
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code == requests.codes.ok:
            return True
        else:
            logging.warn("Error publishing model: %s" % r.text)
            return False
