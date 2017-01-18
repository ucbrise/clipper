from __future__ import print_function, with_statement
from fabric.api import *
# from fabric.colors import green as _green, yellow as _yellow
# from fabric.contrib.console import confirm
from fabric.contrib.files import append
# from fabric.contrib.project import rsync_project
# from StringIO import StringIO
# import sys
import os
import requests
import json
import yaml
# import gevent
# import traceback
# import toml
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

aws_cli_config = """
[default]
region = us-east-1
aws_access_key_id = {access_key}
aws_secret_access_key = {secret_key}
"""

# TODO TODO TODO: change images from :test to :lates
# before accepting PR
DOCKER_COMPOSE_DICT = {
    'networks': {
        'default': {
            'external': {
                'name': DOCKER_NW}}},
    'services': {
        'mgmt_frontend': {
            'command': [
                '--redis_ip=redis',
                '--redis_port=6379'],
            'depends_on': ['redis'],
            'image': 'clipper/management_frontend:test',
            'ports': ['1338:1338']},
        'query_frontend': {
            'command': [
                '--redis_ip=redis',
                '--redis_port=6379'],
            'depends_on': [
                'redis',
                'mgmt_frontend'],
            'image': 'clipper/query_frontend:test',
            'ports': [
                '7000:7000',
                '1337:1337']},
        'redis': {
            'image': 'redis:alpine',
            'ports': ['6379:6379']}},
    'version': '2'
}


class Cluster:

    def __init__(self, host, user, key_path):
        # global env
        env.key_filename = key_path
        env.user = user
        env.host_string = host
        self.host = host
        # Make sure docker is running on cluster
        with hide("warnings", "output", "running"):
            print("Checking if Docker is running...")
            sudo("docker ps")
            # print("Found Docker running")
            # print("Checking if docker-compose is installed...")
            dc_installed = sudo("docker-compose --version", warn_only=True)
            # print("Found docker-compose installed")
            if dc_installed.return_code != 0:
                print("docker-compose not installed on host.")
                print("attempting to install it")
                sudo("curl -L https://github.com/docker/compose/releases/"
                     "download/1.10.0-rc1/docker-compose-`uname -s`-`uname -m` "
                     "> /usr/local/bin/docker-compose")
                sudo("chmod +x /usr/local/bin/docker-compose")

            # print("Creating internal Docker network")
            nw_create_command = ("docker network create --driver bridge {nw}"
                                 .format(nw=DOCKER_NW))
            sudo(nw_create_command, warn_only=True)
            run("mkdir -p {model_repo}".format(model_repo=MODEL_REPO))
            # print("Creating local model repository")

    def start_clipper(self):
        with hide("output", "warnings", "running"):
            run("rm -f docker-compose.yml")
            append(
                "docker-compose.yml",
                yaml.dump(
                    DOCKER_COMPOSE_DICT,
                    default_flow_style=False))
            sudo("docker-compose up -d query_frontend")
            print("Clipper is running")

    def register_application(self, **kwargs):
        name = kwargs.pop('name')
        candidate_models = kwargs.pop('candidate_models')
        input_type = kwargs.pop('input_type')
        output_type = kwargs.pop('output_type', "double")
        selection_policy = kwargs.pop('selection_policy', "bandit_policy")
        slo_micros = kwargs.pop('slo_micros', 20000)
        if kwargs:
            raise TypeError('Unexpected **kwargs: %r' % kwargs)
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
        with hide("output", "running"):
            result = local(("redis-cli -h {host} -p 6379 -n {db} keys \"*\""
                            .format(host=self.host,
                                    db=REDIS_APPLICATION_DB_NUM)),
                           capture=True)

            if len(result.stdout) > 0:
                print(result.stdout)
                # splits = result.stdout.split("\n")
                # fmt_result = dict([(splits[i], splits[i+1])
                #                 for i in range(0, len(splits), 2)])
                # pp = pprint.PrettyPrinter(indent=2)
                # pp.pprint(fmt_result)
            else:
                print("Clipper has no applications registered")

    def get_app_info(self, name):
        with hide("output", "running"):
            result = local("redis-cli -h {host} -p 6379 -n {db} hgetall {name}".format(
                host=self.host, name=name, db=REDIS_APPLICATION_DB_NUM), capture=True)

            if len(result.stdout) > 0:
                splits = result.stdout.split("\n")
                fmt_result = dict([(splits[i], splits[i+1])
                                for i in range(0, len(splits), 2)])
                pp = pprint.PrettyPrinter(indent=2)
                pp.pprint(fmt_result)
            else:
                warn("Application \"%s\" not found" % name)

    def get_bandit_weights(self, **kwargs):
        app_name = kwargs.pop('app_name')
        uid = kwargs.pop('uid')
        if kwargs:
            raise TypeError('Unexpected **kwargs: %r' % kwargs)

        url = "http://%s:1338/admin/get_state" % self.host
        req_json = json.dumps({
            "app_name": app_name,
            "uid": uid,
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        raw_weights = r.text
        def extract_model(m):
            ps = m.split(":")
            print(ps[0] + ": " + ps[2])
            return ((ps[0]+":"+ps[1]), float(ps[2]))
        weights = [extract_model(m) for m in raw_weights.split(",")]
        return weights


    def deploy_model(self, **kwargs):
        """
        Add a model model to Clipper.

        Args:
            name(str):      The name to assign this model.
            version(int):   The version to assign this model.
            model(str or BaseEstimator): The trained model to add to Clipper.
                This can either be the Scikit-Learn trained model object, or
                a path to a serialized Scikit-Learn model that was serialized
                using the joblib library. If you provide a path, model should
                be the path to the directory container all of the *.pkl and *.npy
                files, not the path to the *.pkl file itself.
            container_name(str): The Docker container image to use to run this model container.
            labels(list of str): A set of strings annotating the model
            num_containers(int optional): The number of replicas of the model to create. You can
            also create more replicas later.
        """

        name = kwargs.pop('name')
        version = kwargs.pop('version')
        model_data = kwargs.pop('model_data')
        container_name = kwargs.pop('container_name')
        labels = kwargs.pop('labels')
        input_type = kwargs.pop('input_type')
        num_containers = kwargs.pop('num_containers', 1)

        if kwargs:
            raise TypeError('Unexpected **kwargs: %r' % kwargs)

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

            if (not self.put_container_on_host(container_name)):
                return False
            # print("Container %s available on host" % container_name)

            # Put model parameter data on host
            vol = "{model_repo}/{name}/{version}".format(
                model_repo=MODEL_REPO, name=name, version=version)
            # print(vol)
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
            if not self.publish_new_model(name, version, labels, input_type,
                                        container_name,
                                        os.path.join(vol, os.path.basename(model_data_path))):
                return False
            else:
                print("Published model to Clipper")
                # aggregate results of starting all containers
                return all([self.add_container(name, version)
                            for r in range(num_containers)])

    def add_container(self, model_name, model_version):
        """
        Starts a new container for a model that has already been added to
        Clipper. This method will fail if you have not already called
        Cluster.add_model() for the provided name and version.
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
            model_metadata = dict([(splits[i].strip(),
                                    splits[i + 1].strip())
                                for i in range(0, len(splits),
                                                2)])
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

    def publish_new_model(
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

    def put_container_on_host(self, container_name):
        """
        Puts the provided container on the host. This method is safe to call
        multiple times with the same container name. Subsequent calls will
        detect that the container is already present on the host and do
        nothing.

        Args:
            container_name(str): The name of the container. This method will
            first check the host, then Docker Hub, then the local machine to
            find the container.

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


    def stop_all(self):
        print("Stopping Clipper and all running models...")
        with hide("output", "warnings", "running"):
            sudo("docker-compose stop", warn_only=True)
            sudo("docker stop $(docker ps -a -q)", warn_only=True)
            sudo("docker rm $(docker ps -a -q)", warn_only=True)

    def cleanup(self):
        with hide("output", "warnings", "running"):
            self.stop_all()
            run("rm -rf {model_repo}".format(model_repo=MODEL_REPO))
            sudo("docker rmi --force $(docker images -q)", warn_only=True)
            sudo("docker network rm clipper_nw", warn_only=True)

    def pull_docker_images(self):
        with hide("output"):
            sudo("docker pull clipper/query_frontend:test")
            sudo("docker pull clipper/management_frontend:test")
            sudo("docker pull redis")
            sudo("docker pull clipper/sklearn_cifar_container:test")
            sudo("docker pull clipper/tf_cifar_container:test")

############################################################################

    def get_metrics(self):
        # for h in self.hosts:
        url = "http://%s:1337/metrics" % self.host
        r = requests.get(url)
        try:
            # s = json.dumps(r.json(), indent=4)
            s = r.json()
        except TypeError:
            s = r.text
        return s
