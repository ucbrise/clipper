from __future__ import print_function, with_statement
from fabric.api import *
from fabric.contrib.files import append
import os
import requests
import json
import yaml
import pprint
import subprocess32 as subprocess
import shutil
from sklearn import base
from sklearn.externals import joblib
from cStringIO import StringIO
import sys
sys.path.insert(0, os.path.abspath('../../containers/python/'))
from pywrencloudpickle import CloudPickler

MODEL_REPO = "/tmp/clipper-models"
DOCKER_NW = "clipper_nw"

REDIS_STATE_DB_NUM = 1
REDIS_MODEL_DB_NUM = 2
REDIS_CONTAINER_DB_NUM = 3
REDIS_RESOURCE_DB_NUM = 4
REDIS_APPLICATION_DB_NUM = 5

REDIS_PORT = 6379
CLIPPER_QUERY_PORT = 1337
CLIPPER_MANAGEMENT_PORT = 1338
CLIPPER_RPC_PORT = 7000

aws_cli_config = """
[default]
region = us-east-1
aws_access_key_id = {access_key}
aws_secret_access_key = {secret_key}
"""

DOCKER_COMPOSE_DICT = {
    'networks': {
        'default': {
            'external': {
                'name': DOCKER_NW
            }
        }
    },
    'services': {
        'mgmt_frontend': {
            'command': ['--redis_ip=redis', '--redis_port=%d' % REDIS_PORT],
            'depends_on': ['redis'],
            'image': 'clipper/management_frontend:latest',
            'ports':
            ['%d:%d' % (CLIPPER_MANAGEMENT_PORT, CLIPPER_MANAGEMENT_PORT)]
        },
        'query_frontend': {
            'command': ['--redis_ip=redis', '--redis_port=%d' % REDIS_PORT],
            'depends_on': ['redis', 'mgmt_frontend'],
            'image':
            'clipper/query_frontend:latest',
            'ports': [
                '%d:%d' % (CLIPPER_RPC_PORT, CLIPPER_RPC_PORT),
                '%d:%d' % (CLIPPER_QUERY_PORT, CLIPPER_QUERY_PORT)
            ]
        },
        'redis': {
            'image': 'redis:alpine',
            'ports': ['%d:%d' % (REDIS_PORT, REDIS_PORT)]
        }
    },
    'version': '2'
}

LOCAL_HOST_NAMES = ["local", "localhost", "127.0.0.1"]

EXTERNALLY_MANAGED_MODEL = "EXTERNAL"


class Clipper:
    """
    Connection to a Clipper instance for administrative purposes.

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

    Sets up the machine for running Clipper. This includes verifying
    SSH credentials and initializing Docker.

    Docker and docker-compose must already by installed on the machine
    before connecting to a machine.
    """

    def __init__(self, host, user=None, key_path=None, sudo=False,
                 ssh_port=22):
        self.sudo = sudo
        self.host = host
        if self._host_is_local():
            self.host = "localhost"
            env.host_string = self.host
        else:
            if not user or not key_path:
                print(
                    "user and key_path must be specified when instantiating Clipper with a nonlocal host"
                )
                raise
            env.user = user
            env.key_filename = key_path
            env.host_string = "%s:%d" % (host, ssh_port)
        # Make sure docker is running on cluster
        self._start_docker_if_necessary()

    def _host_is_local(self):
        return self.host in LOCAL_HOST_NAMES

    def _start_docker_if_necessary(self):
        with hide("warnings", "output", "running"):
            print("Checking if Docker is running...")
            self._execute_root("docker ps")
            dc_installed = self._execute_root(
                "docker-compose --version", warn_only=True)
            if dc_installed.return_code != 0:
                print("docker-compose not installed on host.")
                print("attempting to install it")
                self._execute_root(
                    "curl -L https://github.com/docker/compose/releases/"
                    "download/1.10.0-rc1/docker-compose-`uname -s`-`uname -m` "
                    "> /usr/local/bin/docker-compose")
                self._execute_root("chmod +x /usr/local/bin/docker-compose")
            nw_create_command = ("docker network create --driver bridge {nw}"
                                 .format(nw=DOCKER_NW))
            self._execute_root(nw_create_command, warn_only=True)
            self._execute_standard(
                "mkdir -p {model_repo}".format(model_repo=MODEL_REPO))

    def _execute_root(self, *args, **kwargs):
        if not self.sudo:
            return self._execute_standard(*args, **kwargs)
        elif self._host_is_local():
            return self._execute_local(*args, **kwargs)
        else:
            return sudo(*args, **kwargs)

    def _execute_standard(self, *args, **kwargs):
        if self._host_is_local():
            return self._execute_local(*args, **kwargs)
        else:
            return run(*args, **kwargs)

    def _execute_local(self, *args, **kwargs):
        # local is not currently capable of simultaneously printing and
        # capturing output, as run/sudo do. The capture kwarg allows you to
        # switch between printing and capturing as necessary, and defaults to
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
                    self._copytree(local_path, remote_path)
                else:
                    shutil.copy2(local_path, remote_path)
        else:
            put(
                local_path=local_path,
                remote_path=remote_path,
                *args,
                **kwargs)

    # Taken from http://stackoverflow.com/a/12514470
    # Recursively copies a directory from src to dst,
    # where dst may or may not exist. We cannot use
    # shutil.copytree() alone because it stipulates that
    # dst cannot already exist
    def _copytree(self, src, dst, symlinks=False, ignore=None):
        # Appends the final directory in the tree specified by "src" to
        # the tree specified by "dst". This is consistent with fabric's
        # put() behavior
        final_dst_char = dst[len(dst) - 1]
        if final_dst_char != "/":
            dst = dst + "/"
        nested_dir_names = src.split("/")
        dst = dst + nested_dir_names[len(nested_dir_names) - 1]

        if not os.path.exists(dst):
            os.makedirs(dst)
        for item in os.listdir(src):
            s = os.path.join(src, item)
            d = os.path.join(dst, item)
            if os.path.isdir(s):
                self._copytree(s, d, symlinks, ignore)
            else:
                if not os.path.exists(
                        d) or os.stat(s).st_mtime - os.stat(d).st_mtime > 1:
                    shutil.copy2(s, d)

    def start(self):
        """Start a Clipper instance.

        """
        with hide("output", "warnings", "running"):
            self._execute_standard("rm -f docker-compose.yml")
            self._execute_append("docker-compose.yml",
                                 yaml.dump(
                                     DOCKER_COMPOSE_DICT,
                                     default_flow_style=False))
            self._execute_root("docker-compose up -d query_frontend")
            print("Clipper is running")

    def register_application(self,
                             name,
                             model,
                             input_type,
                             default_output,
                             slo_micros=20000):
        """Register a new Clipper application.

        Parameters
        ----------
        name : str
            The name of the application.
        model : str
            The name of the model this application will query.
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        default_output : float
            The default prediction to use if the model does not return a prediction
            by the end of the latency objective.
        slo_micros : int, optional
            The query latency objective for the application in microseconds.
            Default is 20,000 (20 ms).
        """
        url = "http://%s:%d/admin/add_app" % (self.host,
                                              CLIPPER_MANAGEMENT_PORT)
        req_json = json.dumps({
            "name": name,
            "candidate_model_names": [model],
            "input_type": input_type,
            "default_output": str(default_output),
            "latency_slo_micros": slo_micros
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        print(r.text)

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
        url = "http://%s:1338/admin/get_all_applications$" % self.host
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.get(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            print(r.text)
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
        r = requests.get(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                return None
            return app_info
        else:
            print(r.text)
            return None

    def inspect_selection_policy(self, app_name, uid):
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

    def register_external_model(self, name, version, labels, input_type):
        """Registers a model with Clipper without deploying it in any containers.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        labels : list of str
            A set of strings annotating the model
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        """
        return self._publish_new_model(name, version, labels, input_type,
                                       EXTERNALLY_MANAGED_MODEL,
                                       EXTERNALLY_MANAGED_MODEL)

    def deploy_predict_function(self,
                                name,
                                version,
                                predict_function,
                                labels,
                                input_type,
                                num_containers=1):
        """Deploy an arbitrary Python function to Clipper.

        The function should take a list of inputs of the type specified by `input_type` and
        return a Python or numpy array of predictions. All dependencies for the function must
        be installed with Anaconda or Pip and this function must be called from within an Anaconda
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
        labels : list of str
            A set of strings annotating the model
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        num_containers : int, optional
            The number of replicas of the model to create. More replicas can be
            created later as well. Defaults to 1.

        Example
        -------
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
                ["example"],
                "doubles",
                num_containers=1)
        """

        relative_base_serializations_dir = "predict_serializations"
        default_python_container = "clipper/python-container"
        predict_fname = "predict_func.pkl"
        environment_fname = "environment.yml"

        base_serializations_dir = os.path.abspath(
            relative_base_serializations_dir)

        # Serialize function
        s = StringIO()
        c = CloudPickler(s, 2)
        c.dump(predict_function)
        serialized_prediction_function = s.getvalue()

        # Set up serialization directory
        serialization_dir = "{base}/{dir}".format(
            base=base_serializations_dir, dir=name)
        if not os.path.exists(serialization_dir):
            os.makedirs(serialization_dir)

        # Write out function serialization
        func_file_path = "{dir}/{predict_fname}".format(
            dir=serialization_dir, predict_fname=predict_fname)
        with open(func_file_path, "w") as serialized_function_file:
            serialized_function_file.write(serialized_prediction_function)
        print("Serialized and supplied predict function")

        # Export Anaconda environment
        subprocess.call(
            "PIP_FORMAT=legacy conda env export >> {environment_fname}".format(
                environment_fname=environment_fname),
            shell=True)

        # Give container environment details
        shutil.copy(environment_fname, serialization_dir)
        print("Supplied environment details")

        # Deploy function
        return self.deploy_model(name, version, serialization_dir,
                                 default_python_container, labels, input_type,
                                 num_containers)

    def deploy_model(self,
                     name,
                     version,
                     model_data,
                     container_name,
                     labels,
                     input_type,
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
        labels : list of str
            A set of strings annotating the model
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        num_containers : int, optional
            The number of replicas of the model to create. More replicas can be
            created later as well. Defaults to 1.
        """
        with hide("warnings", "output", "running"):
            if isinstance(model_data, base.BaseEstimator):
                fname = name.replace("/", "_")
                pkl_path = '/tmp/%s/%s.pkl' % (fname, fname)
                model_data_path = "/tmp/%s" % fname
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
            # publish model to Clipper and verify success before copying model
            # parameters to Clipper and starting containers
            if not self._publish_new_model(
                    name, version, labels, input_type, container_name,
                    os.path.join(vol, os.path.basename(model_data_path))):
                return False
            print("Published model to Clipper")

            if (not self._put_container_on_host(container_name)):
                return False

            # Put model parameter data on host
            with hide("warnings", "output", "running"):
                self._execute_standard("mkdir -p {vol}".format(vol=vol))

            with cd(vol):
                with hide("warnings", "output", "running"):
                    if model_data_path.startswith("s3://"):
                        with hide("warnings", "output", "running"):
                            aws_cli_installed = self._execute_standard(
                                "dpkg-query -Wf'${db:Status-abbrev}' awscli 2>/dev/null | grep -q '^i'",
                                warn_only=True).return_code == 0
                            if not aws_cli_installed:
                                self._execute_root("apt-get update -qq")
                                self._execute_root(
                                    "apt-get install -yqq awscli")
                            if self._execute_root(
                                    "stat ~/.aws/config",
                                    warn_only=True).return_code != 0:
                                self._execute_standard("mkdir -p ~/.aws")
                                self._execute_append(
                                    "~/.aws/config",
                                    aws_cli_config.format(
                                        access_key=os.environ[
                                            "AWS_ACCESS_KEY_ID"],
                                        secret_key=os.environ[
                                            "AWS_SECRET_ACCESS_KEY"]))

                        self._execute_standard(
                            "aws s3 cp {model_data_path} {dl_path} --recursive".
                            format(
                                model_data_path=model_data_path,
                                dl_path=os.path.join(
                                    vol, os.path.basename(model_data_path))))
                    else:
                        with hide("output", "running"):
                            self._execute_put(model_data_path, vol)

            print("Copied model data to host")
            # aggregate results of starting all containers
            return all([
                self.add_container(name, version)
                for r in range(num_containers)
            ])

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
        """
        with hide("warnings", "output", "running"):
            # Look up model info in Redis
            model_key = "{mn}:{mv}".format(mn=model_name, mv=model_version)
            result = local(
                "redis-cli -h {host} -p 6379 -n {db} hgetall {key}".format(
                    host=self.host, key=model_key, db=REDIS_MODEL_DB_NUM),
                capture=True)

            if "nil" in result.stdout:
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

            # TODO: don't try to add container if it's an external container
            if image_name is not EXTERNALLY_MANAGED_MODEL:
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
                result = self._execute_root(add_container_cmd)
                return result.return_code == 0
            else:
                print("Cannot start containers for externally managed model %s"
                      % model_name)
                return True

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
        url = "http://%s:%d/admin/set_model_version" % (
            self.host, CLIPPER_MANAGEMENT_PORT)
        req_json = json.dumps({
            "model_name": model_name,
            "model_version": model_version
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        print(r.text)
        for r in range(num_containers):
            self.add_container(model_name, model_version)

    def stop_all(self):
        """Stops and removes all Docker containers on the host.

        """
        print("Stopping Clipper and all running models...")
        with hide("output", "warnings", "running"):
            self._execute_root("docker-compose stop", warn_only=True)
            self._execute_root(
                "docker stop $(docker ps -a -q)", warn_only=True)
            self._execute_root("docker rm $(docker ps -a -q)", warn_only=True)

    def cleanup(self):
        """Cleans up all Docker artifacts.

        This will stop and remove all Docker containers and images
        from the host and destroy the Docker network Clipper uses.
        """
        with hide("output", "warnings", "running"):
            self.stop_all()
            self._execute_standard(
                "rm -rf {model_repo}".format(model_repo=MODEL_REPO))
            self._execute_root(
                "docker rmi --force $(docker images -q)", warn_only=True)
            self._execute_root("docker network rm clipper_nw", warn_only=True)

    def _publish_new_model(self, name, version, labels, input_type,
                           container_name, model_data_path):
        url = "http://%s:%d/admin/add_model" % (self.host,
                                                CLIPPER_MANAGEMENT_PORT)
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
            print("Error publishing model: %s" % r.text)
            return False

    def _put_container_on_host(self, container_name):
        """Puts the provided container on the host.

        Parameters
        __________
        container_name : str
            The name of the container.

        Notes
        -----
        This method will first check the host, then Docker Hub, then the local
        machine to find the container.

        This method is safe to call multiple times with the same container name.
        Subsequent calls will detect that the container is already present on
        the host and do nothing.

        """
        with hide("output", "warnings", "running"):
            # first see if container is already present on host
            host_result = self._execute_root(
                "docker images -q {cn}".format(cn=container_name))
            if len(host_result.stdout) > 0:
                print("Found %s on host" % container_name)
                return True
            # now try to pull from Docker Hub
            hub_result = self._execute_root(
                "docker pull {cn}".format(cn=container_name), warn_only=True)
            if hub_result.return_code == 0:
                print("Found %s in Docker hub" % container_name)
                return True

            # assume container_name refers to a local container and
            # copy it to host
            local_result = local(
                "docker images -q {cn}".format(cn=container_name))

            if len(local_result.stdout) > 0:
                saved_fname = container_name.replace("/", "_")
                subprocess.call("docker save -o /tmp/{fn}.tar {cn}".format(
                    fn=saved_fname, cn=container_name))
                tar_loc = "/tmp/{fn}.tar".format(fn=saved_fname)
                self._execute_put(tar_loc, tar_loc)
                self._execute_root("docker load -i {loc}".format(loc=tar_loc))
                # self._execute_root("docker tag {image_id} {cn}".format(
                #       image_id=image_id, cn=cn))
                # now check to make sure we can access it
                host_result = self._execute_root(
                    "docker images -q {cn}".format(cn=container_name))
                if len(host_result.stdout) > 0:
                    print("Successfuly copied %s to host" % container_name)
                    return True
                else:
                    warn("Problem copying container %s to host" %
                         container_name)
                    return False

            # out of options
            warn("Could not find %s, please try with a valid "
                 "container docker image")
            return False
