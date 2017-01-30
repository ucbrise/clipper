from __future__ import print_function, with_statement
from fabric.api import *
from fabric.contrib.files import append
import os
import requests
import json
import yaml
import pprint
import subprocess32 as subprocess
from sklearn import base
from sklearn.externals import joblib

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
                'name': DOCKER_NW}}},
    'services': {
        'mgmt_frontend': {
            'command': [
                '--redis_ip=redis',
                '--redis_port=%d' % REDIS_PORT],
            'depends_on': ['redis'],
            'image': 'clipper/management_frontend:latest',
            'ports': ['%d:%d' % (CLIPPER_MANAGEMENT_PORT, CLIPPER_MANAGEMENT_PORT)]},
        'query_frontend': {
            'command': [
                '--redis_ip=redis',
                '--redis_port=%d' % REDIS_PORT],
            'depends_on': [
                'redis',
                'mgmt_frontend'],
            'image': 'clipper/query_frontend:latest',
            'ports': [
                '%d:%d' % (CLIPPER_RPC_PORT, CLIPPER_RPC_PORT),
                '%d:%d' % (CLIPPER_QUERY_PORT, CLIPPER_QUERY_PORT)]},
        'redis': {
            'image': 'redis:alpine',
            'ports': ['%d:%d' % (REDIS_PORT, REDIS_PORT)]}},
    'version': '2'
}


class Clipper:
    """
    Connection to a Clipper instance for administrative purposes.

    Parameters
    ----------
    host : str
        The hostname of the machine to start Clipper on. The machine
        should allow passwordless SSH access.
    user : str
        The SSH username.
    key_path : str
        The path to the SSH private key.

    Sets up the machine for running Clipper. This includes verifying
    SSH credentials and initializing Docker.

    Docker and docker-compose must already by installed on the machine
    before connecting to a machine.
    """

    def __init__(self, host, user, key_path):
        env.key_filename = key_path
        env.user = user
        env.host_string = host
        self.host = host
        # Make sure docker is running on cluster
        with hide("warnings", "output", "running"):
            print("Checking if Docker is running...")
            sudo("docker ps")
            dc_installed = sudo("docker-compose --version", warn_only=True)
            if dc_installed.return_code != 0:
                print("docker-compose not installed on host.")
                print("attempting to install it")
                sudo("curl -L https://github.com/docker/compose/releases/"
                     "download/1.10.0-rc1/docker-compose-`uname -s`-`uname -m` "
                     "> /usr/local/bin/docker-compose")
                sudo("chmod +x /usr/local/bin/docker-compose")
            nw_create_command = ("docker network create --driver bridge {nw}"
                                 .format(nw=DOCKER_NW))
            sudo(nw_create_command, warn_only=True)
            run("mkdir -p {model_repo}".format(model_repo=MODEL_REPO))

    def start(self):
        """Start a Clipper instance.

        """
        with hide("output", "warnings", "running"):
            run("rm -f docker-compose.yml")
            append(
                "docker-compose.yml",
                yaml.dump(
                    DOCKER_COMPOSE_DICT,
                    default_flow_style=False))
            sudo("docker-compose up -d query_frontend")
            print("Clipper is running")

    def register_application(self,
                             name,
                             candidate_models,
                             input_type,
                             selection_policy,
                             output_type="double",
                             slo_micros=20000):
        """Register a new Clipper application.

        Parameters
        ----------
        name : str
            The name of the application.
        candidate_models : list of dict
            The list of models this application will attempt to query.
            Each candidate model is defined as a dict with keys `model_name`
            and `model_version`. Example::
                candidate_models = [
                    {"model_name": "my_model", "model_version": 1},
                    {"model_name": "other_model", "model_version": 1},
                ]
        input_type : str
            One of "integers", "floats", "doubles", "bytes", or "strings".
        selection_policy : str
            The name of the model selection policy to be used for the
            application.
        output_type : str, optional
            Either "double" or "int". Default is "double".
        slo_micros : int, optional
            The query latency objective for the application in microseconds.
            Default is 20,000 (20 ms).
        """
        url = "http://%s:1338/admin/add_app" % self.host
        req_json = json.dumps({
            "name": name,
            "candidate_models": candidate_models,
            "input_type": input_type,
            "output_type": output_type,
            "selection_policy": selection_policy,
            "latency_slo_micros": slo_micros
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        print(r.text)

    def list_apps(self):
        """List the names of all applications registered with Clipper.

        Returns
        -------
        str
            The string describing each registered application. If no
            applications are found, an empty string is returned.
        """
        with hide("output", "running"):
            result = local(("redis-cli -h {host} -p 6379 -n {db} keys \"*\""
                            .format(host=self.host,
                                    db=REDIS_APPLICATION_DB_NUM)),
                           capture=True)

            if len(result.stdout) > 0:
                return result.stdout
            else:
                print("Clipper has no applications registered")
                return ""

    def get_app_info(self, name):
        """Gets detailed information about a registered application.

        Parameters
        ----------
        name : str
            The name of the application to look up

        Returns
        -------
        dict or None
            Returns a dict with the application info if found. If the application
            is not registered, None is returned.
        """
        with hide("output", "running"):
            result = local("redis-cli -h {host} -p 6379 -n {db} hgetall {name}".format(
                host=self.host, name=name, db=REDIS_APPLICATION_DB_NUM), capture=True)

            if len(result.stdout) > 0:
                splits = result.stdout.split("\n")
                fmt_result = dict([(splits[i], splits[i+1])
                                   for i in range(0, len(splits), 2)])
                pp = pprint.PrettyPrinter(indent=2)
                pp.pprint(fmt_result)
                return fmt_result
            else:
                warn("Application \"%s\" not found" % name)
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

        url = "http://%s:1338/admin/get_state" % self.host
        req_json = json.dumps({
            "app_name": app_name,
            "uid": uid,
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        return r.text

    def deploy_model(self, name, version, model, container_name, labels, num_containers=1):
        """Add a model to Clipper.

        Parameters
        ----------
        name : str
            The name to assign this model.
        version : int
            The version to assign this model.
        model : str or BaseEstimator
            The trained model to add to Clipper. This can either be a
            Scikit-Learn trained model object (an instance of BaseEstimator),
            or a path to a serialized model. Note that many model serialization
            formats split the model across multiple files (e.g. definition file
            and weights file or files). If this is the case, model must be a path
            to the root of a directory tree containing ALL the needed files.
            Depending on the model serialization library you use, this may or may not
            be the path you provided to the serialize method call.
        container_name : str
            The Docker container image to use to run this model container.
        labels : list of str
            A set of strings annotating the model
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
                warn("%s is invalid model format" % str(type(model)))
                return False

            if (not self._put_container_on_host(container_name)):
                return False

            # Put model parameter data on host
            vol = "{model_repo}/{name}/{version}".format(
                model_repo=MODEL_REPO, name=name, version=version)
            with hide("warnings", "output", "running"):
                run("mkdir -p {vol}".format(vol=vol))

            with cd(vol):
                with hide("warnings", "output", "running"):
                    if model_data_path.startswith("s3://"):
                        with hide("warnings", "output", "running"):
                            aws_cli_installed = run(
                                "dpkg-query -Wf'${db:Status-abbrev}' awscli 2>/dev/null | grep -q '^i'",
                                warn_only=True).return_code == 0
                            if not aws_cli_installed:
                                sudo("apt-get update -qq")
                                sudo("apt-get install -yqq awscli")
                            if sudo(
                                    "stat ~/.aws/config",
                                    warn_only=True).return_code != 0:
                                run("mkdir -p ~/.aws")
                                append("~/.aws/config", aws_cli_config.format(
                                    access_key=os.environ["AWS_ACCESS_KEY_ID"],
                                    secret_key=os.environ["AWS_SECRET_ACCESS_KEY"]))

                        run("aws s3 cp {model_data_path} {dl_path} --recursive".format(
                                model_data_path=model_data_path, dl_path=os.path.join(
                                    vol, os.path.basename(model_data_path))))
                    else:
                        with hide("output", "running"):
                            put(model_data_path, ".")

            print("Copied model data to host")
            if not self._publish_new_model(name, version, labels, input_type,
                                           container_name,
                                           os.path.join(vol, os.path.basename(model_data_path))):
                return False
            else:
                print("Published model to Clipper")
                # aggregate results of starting all containers
                return all([self.add_container(name, version)
                            for r in range(num_containers)])

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
            result = sudo(add_container_cmd)
            return result.return_code == 0

    def inspect_instance(self):
        """Fetches metrics from the running Clipper instance.

        Returns
        -------
        str
            The JSON string containing the current set of metrics
            for this instance. On error, the string will be an error message
            (not JSON formatted).
        """
        url = "http://%s:1337/metrics" % self.host
        r = requests.get(url)
        try:
            s = r.json()
        except TypeError:
            s = r.text
        return s

    def stop_all(self):
        """Stops and removes all Docker containers on the host.

        """
        print("Stopping Clipper and all running models...")
        with hide("output", "warnings", "running"):
            sudo("docker-compose stop", warn_only=True)
            sudo("docker stop $(docker ps -a -q)", warn_only=True)
            sudo("docker rm $(docker ps -a -q)", warn_only=True)

    def cleanup(self):
        """Cleans up all Docker artifacts.

        This will stop and remove all Docker containers and images
        from the host and destroy the Docker network Clipper uses.
        """
        with hide("output", "warnings", "running"):
            self.stop_all()
            run("rm -rf {model_repo}".format(model_repo=MODEL_REPO))
            sudo("docker rmi --force $(docker images -q)", warn_only=True)
            sudo("docker network rm clipper_nw", warn_only=True)

    def _publish_new_model(
            self,
            name,
            version,
            labels,
            input_type,
            container_name,
            model_data_path,
            output_type="double"):
        url = "http://%s:1338/admin/add_model" % self.host
        req_json = json.dumps({
            "model_name": name,
            "model_version": version,
            "labels": labels,
            "input_type": input_type,
            "output_type": output_type,
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
            host_result = sudo(
                "docker images -q {cn}".format(cn=container_name))
            if len(host_result.stdout) > 0:
                print("Found %s on host" % container_name)
                return True
            # now try to pull from Docker Hub
            hub_result = sudo("docker pull {cn}".format(cn=container_name),
                              warn_only=True)
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
                    fn=saved_fname,
                    cn=container_name))
                tar_loc = "/tmp/{fn}.tar".format(fn=saved_fname)
                put(tar_loc, tar_loc)
                sudo("docker load -i {loc}".format(loc=tar_loc))
                # sudo("docker tag {image_id} {cn}".format(
                #       image_id=image_id, cn=cn))
                # now check to make sure we can access it
                host_result = sudo(
                    "docker images -q {cn}".format(cn=container_name))
                if len(host_result.stdout) > 0:
                    print("Successfuly copied %s to host" % container_name)
                    return True
                else:
                    warn(
                        "Problem copying container %s to host" %
                        container_name)
                    return False

            # out of options
            warn("Could not find %s, please try with a valid "
                 "container docker image")
            return False
