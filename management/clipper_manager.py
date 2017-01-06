from __future__ import print_function, with_statement
from fabric.api import *
from fabric.colors import green as _green, yellow as _yellow
from fabric.contrib.console import confirm
from fabric.contrib.files import append
from StringIO import StringIO
import sys
import os
import requests
import json
import gevent
import traceback
import toml
import subprocess32 as subprocess
from sklearn import base
from sklearn.externals import joblib

MODEL_REPO = "/tmp/clipper-models"
DOCKER_NW = "clipper_nw"
CLIPPER_METADATA_FILE = "clipper_metadata.json"

aws_cli_config = """
[default]
region = us-east-1
aws_access_key_id = {access_key}
aws_secret_access_key = {secret_key}
"""


class Cluster:

    def __init__(self, host, user, key_path):
        # global env
        env.key_filename = key_path
        env.user = user
        env.host_string = host
        self.host = host
        # Make sure docker is running on cluster
        with hide("warnings", "output", "running"):
            print("Checking if Docker running...")
            sudo("docker ps")
            print("Found Docker running")
            nw_create_command = "docker network create --driver bridge {nw}".format(
                nw=DOCKER_NW)
            sudo(nw_create_command, warn_only=True)
            print("Creating internal Docker network")
            run("mkdir -p {model_repo}".format(model_repo=MODEL_REPO))
            print("Creating local model repository")

    def start_clipper(self, config=None):
        with hide("output"):
            redis_ip = "redis_clipper"
            sudo(
                "docker run -d --network={nw} -p 6379:6379 "
                "--cpuset-cpus=\"0\" --name {redis_ip} redis:alpine".format(
                    nw=DOCKER_NW, redis_ip=redis_ip))

            # start query frontend
            sudo(
                "docker run -d --network={nw} -p 1337:1337 "
                "--cpuset-cpus=\"{min_core}-{max_core}\" --name query_frontend "
                "clipper/query_frontend --redis_ip={redis_ip} --redis_port=6379".format(
                    nw=DOCKER_NW, min_core=1, max_core=4, redis_ip=redis_ip))

            # start management frontend
            sudo(
                "docker run -d --network={nw} -p 1337:1337 "
                "--cpuset-cpus=\"{min_core}-{max_core}\" --name management_frontend "
                "clipper/management_frontend --redis_ip={redis_ip} --redis_port=6379".format(
                    nw=DOCKER_NW, min_core=1, max_core=4, redis_ip=redis_ip))

    def add_replicas(self, name, version, num_replicas=1):
        print(
            "Adding {nr} replicas of model: {model}".format(
                nr=num_replicas,
                model=name))
        vol = "{model_repo}/{name}/{version}".format(
            model_repo=MODEL_REPO, name=name, version=version)
        present = run("stat {vol}".format(vol=vol), warn_only=True)
        if present.return_code == 0:
            # Look up image name
            fd = StringIO()
            get(os.path.join(vol, CLIPPER_METADATA_FILE), fd)
            metadata = json.loads(fd.getvalue())
            image_name = metadata["image_name"]
            base_name = metadata["base_name"]

            # find the max current replica num
            docker_ps_out = sudo("docker ps")
            print(docker_ps_out)
            rs = []
            nv = "{name}_v{version}".format(name=name, version=version)
            print("XXXXXXXXXXXXXXXXNAME: %s" % nv)
            for line in docker_ps_out.split("\n"):
                line_name = line.split()[-1]
                print(line_name)
                if nv in line_name:
                    rep_num = int(line_name.split("_")[-1].lstrip("r"))
                    rs.append(rep_num)
            next_replica_num = max(rs) + 1
            addrs = []
            # Add new replicas
            for r in range(next_replica_num, next_replica_num + num_replicas):
                container_name = "%s_v%d_r%d" % (name, version, r)
                run_mw_command = "docker run -d --network={nw} --name {name} -v {vol}:/model:ro {image}".format(
                    name=container_name, vol=os.path.join(vol, base_name), nw=DOCKER_NW, image=image_name)
                sudo(
                    "docker stop {name}".format(
                        name=container_name),
                    warn_only=True)
                sudo(
                    "docker rm {name}".format(
                        name=container_name),
                    warn_only=True)
                sudo(run_mw_command)
                addrs.append("{cn}:6001".format(cn=container_name))

            new_replica_data = {
                    "name": name,
                    "version": version,
                    "addrs": addrs
                    }
            self.inform_clipper_new_replica(new_replica_data)
        else:
            print(
                "{model} version {version} not found!".format(
                    model=name, version=version))

    def add_local_model(
        self,
        name,
        image_id,
        container_name,
        data_path,
     replicas=1):
        subprocess.call("docker save -o /tmp/{cn}.tar {image_id}".format(
            cn=container_name,
            image_id=image_id))
        tar_loc = "/tmp/{cn}.tar".format(cn=container_name)
        put(tar_loc, tar_loc)
        sudo("docker load -i {loc}".format(loc=tar_loc))
        sudo("docker tag {image_id} {cn}".format(image_id=image_id, cn=cn))
        self.add_model(name, container_name, data_path, replicas=replicas)

    def add_sklearn_model(self, name, model, replicas=1):
        if isinstance(model, base.BaseEstimator):
            fname = name.replace("/", "_")
            pkl_path = '/tmp/%s/%s.pkl' % (fname, fname)
            data_path = "/tmp/%s" % fname
            try:
                os.mkdir(data_path)
            except OSError:
                pass
                # print("directory already exists. Might overwrite existing file")
            joblib.dump(model, pkl_path)
        elif isinstance(model, str):
            # assume that model is a model_path
            data_path = model
        image_name = "dcrankshaw/clipper-sklearn-mw"
        self.add_model(name, image_name, data_path, replicas=replicas)

    def add_pyspark_model(self, name, data_path, replicas=1):
        image_name = "dcrankshaw/clipper-spark-mw"
        self.add_model(name, image_name, data_path, replicas=replicas)

    def add_model(self, name, image_name, data_path, replicas=1):
        version = 1
        vol = "{model_repo}/{name}/{version}".format(
            model_repo=MODEL_REPO, name=name, version=version)
        print(vol)

        with hide("warnings", "output", "running"):
            run("mkdir -p {vol}".format(vol=vol))

        with cd(vol):
            with hide("warnings", "output", "running"):
                run("rm {metadata}".format(
                    metadata=CLIPPER_METADATA_FILE), warn_only=True)
                append(CLIPPER_METADATA_FILE, json.dumps({
                    "image_name": image_name,
                    "base_name": os.path.basename(data_path),
                    }))
            if data_path.startswith("s3://"):
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

                run("aws s3 cp {data_path} {dl_path} --recursive".format(
                    data_path=data_path,
                    dl_path=os.path.join(vol, os.path.basename(data_path))))
            else:
                with hide("output", "running"):
                    put(data_path, ".")
        addrs = []
        for r in range(replicas):
            container_name = "%s_v%d_r%d" % (name, version, r)
            run_mw_command = "docker run -d --network={nw} --name {name} -v {vol}:/model:ro {image}".format(
                    name=container_name,
                    vol=os.path.join(vol, os.path.basename(data_path)),
                    nw=DOCKER_NW, image=image_name)

            with hide("output", "warnings", "running"):
                sudo(
                    "docker stop {name}".format(
                        name=container_name),
                    warn_only=True)
                sudo(
                    "docker rm {name}".format(
                        name=container_name),
                    warn_only=True)
            with hide("output"):
                sudo(run_mw_command)
            addrs.append("{cn}:6001".format(cn=container_name))
        new_model_data = {
                "name": name,
                "version": version,
                "addrs": addrs
                }
        self.inform_clipper_new_model(new_model_data)

    # def inform_clipper_new_model(self, new_model_data):
    #     url = "http://%s:1337/addmodel" % self.host
    #     req_json = json.dumps(new_model_data)
    #     headers = {'Content-type': 'application/json'}
    #     r = requests.post(url, headers=headers, data=req_json)
    #
    # def inform_clipper_new_replica(self, new_replica_data):
    #     url = "http://%s:1337/addreplica" % self.host
    #     req_json = json.dumps(new_replica_data)
    #     headers = {'Content-type': 'application/json'}
    #     r = requests.post(url, headers=headers, data=req_json)

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

    def get_selection_state_model(self, uid):
        # for h in self.hosts:
        url = "http://%s:1337/correctionmodel" % self.host
        data = {"uid": uid, }
        req_json = json.dumps(data)
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        # print(r.text)
        try:
            # s = json.dumps(r.json(), indent=4)
            s = r.json()
        except TypeError:
            s = r.text
        return s
        # print(json.dumps(r.json(), indent=4))
        return s

    def stop_all(self):
        print("Stopping Clipper and all running models...")
        with hide("output", "warnings", "running"):
            sudo("docker stop $(docker ps -a -q)", warn_only=True)
            sudo("docker rm $(docker ps -a -q)", warn_only=True)
