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

aws_cli_config="""
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
            nw_create_command = "docker network create --driver bridge {nw}".format(nw=DOCKER_NW)
            sudo(nw_create_command, warn_only=True)
            print("Creating internal Docker network")
            run("mkdir -p {model_repo}".format(model_repo=MODEL_REPO))
            print("Creating local model repository")




    def add_replicas(self, name, version, num_replicas=1):
        print("Adding {nr} replicas of model: {model}".format(nr=num_replicas, model=name))
        vol = "{model_repo}/{name}/{version}".format(model_repo=MODEL_REPO, name=name, version=version)
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
                        name=container_name,
                        vol=os.path.join(vol, base_name),
                        nw=DOCKER_NW, image=image_name)
                sudo("docker stop {name}".format(name=container_name), warn_only=True)
                sudo("docker rm {name}".format(name=container_name), warn_only=True)
                sudo(run_mw_command)
                addrs.append("{cn}:6001".format(cn=container_name))

            new_replica_data = {
                    "name": name,
                    "version": version,
                    "addrs": addrs
                    }
            self.inform_clipper_new_replica(new_replica_data)
        else:
            print("{model} version {version} not found!".format(model=name, version=version))

    def add_local_model(self, name, image_id, container_name, data_path, replicas=1):
        subprocess.call("docker save -o /tmp/{cn}.tar {image_id}".format(
            cn=container_name,
            image_id=image_id))
        tar_loc = "/tmp/{cn}.tar".format(cn=container_name)
        put(tar_loc, tar_loc)
        sudo("docker load -i {loc}".format(loc=tar_loc))
        sudo("docker tag {image_id} {cn}".format(image_id = image_id, cn=cn))
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
        vol = "{model_repo}/{name}/{version}".format(model_repo=MODEL_REPO, name=name, version=version)
        print(vol)

        with hide("warnings", "output", "running"):
            run("mkdir -p {vol}".format(vol=vol))

        with cd(vol):
            with hide("warnings", "output", "running"):
                run("rm {metadata}".format(metadata=CLIPPER_METADATA_FILE), warn_only=True)
                append(CLIPPER_METADATA_FILE, json.dumps({
                    "image_name": image_name,
                    "base_name": os.path.basename(data_path),
                    }))
            if data_path.startswith("s3://"):
                with hide("warnings", "output", "running"):
                    aws_cli_installed = run("dpkg-query -Wf'${db:Status-abbrev}' awscli 2>/dev/null | grep -q '^i'", warn_only=True).return_code == 0
                    if not aws_cli_installed:
                        sudo("apt-get update -qq")
                        sudo("apt-get install -yqq awscli")
                    if sudo("stat ~/.aws/config", warn_only=True).return_code != 0:
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
                sudo("docker stop {name}".format(name=container_name), warn_only=True)
                sudo("docker rm {name}".format(name=container_name), warn_only=True)
            with hide("output"):
                sudo(run_mw_command)
            addrs.append("{cn}:6001".format(cn=container_name))
        new_model_data = {
                "name": name,
                "version": version,
                "addrs": addrs
                }
        self.inform_clipper_new_model(new_model_data)

    def inform_clipper_new_model(self, new_model_data):
        url = "http://%s:1337/addmodel" % self.host
        req_json = json.dumps(new_model_data)
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

    def inform_clipper_new_replica(self, new_replica_data):
        url = "http://%s:1337/addreplica" % self.host
        req_json = json.dumps(new_replica_data)
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        
    def start_clipper(self, config=None):
        conf_loc = "/home/ubuntu/conf.toml"
        if config is not None:
            with hide("warnings", "output", "running"):
                put(config, "~/conf.toml")
        else:
            # Use default config
            clipper_conf_dict = {
                    "name" : "clipper-demo",
                    "slo_micros" : 20000,
                    "correction_policy" : "logistic_regression",
                    "use_lsh" : False,
                    "input_type" : "float",
                    "input_length" : 784,
                    "window_size" : -1,
                    "redis_ip" : "redis-clipper",
                    "redis_port" : 6379,
                    "num_predict_workers" : 1,
                    "num_update_workers" : 1,
                    "cache_size" : 49999,
                    "batching": { "strategy": "aimd", "sample_size": 1000},
                    "models": []
                    }
            with hide("output", "warnings", "running"):
                run("rm ~/conf.toml", warn_only=True)
                append("~/conf.toml", toml.dumps(clipper_conf_dict))
            print("starting Clipper with default settings:\n%s" % toml.dumps(clipper_conf_dict))
        with hide("output"):
            sudo("docker run -d --network={nw} -p 6379:6379 "
                    "--cpuset-cpus=\"0\" --name redis-clipper redis:alpine".format(nw=DOCKER_NW))

            sudo("docker run -d --network={nw} -p 1337:1337 "
                    "--cpuset-cpus=\"{min_core}-{max_core}\" --name clipper "
                    "-v ~/conf.toml:/tmp/conf.toml dcrankshaw/clipper".format(nw=DOCKER_NW, min_core=1, max_core=4))

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

    def get_correction_model(self, uid):
        # for h in self.hosts:
        url = "http://%s:1337/correctionmodel" % self.host
        data = { "uid": uid, }
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























#
#
#
# """
# Maybe we should define the abstractions in a Clipper cluster.
#
# What does this look like in local mode????
# """
#
# fake_authenticator = { 'root': 'password' }
#
# def authenticate(user_id, key):
#     print("WARNING, THIS IS FAKE AUTHENTICATION. DO NOT USE!!!!!!!")
#     if fake_authenticator.get(user_id) == key:
#         True
#     else:
#         False
#
# class ClipperManager:
#     """
#         Object to manage Clipper models and versions.
#
#         Questions:
#         + How do rolling updates work?
#             + What is the update granularity? Node, traffic percentage, something else?
#
#
#     """
#
#     def __init__(user_id):
#         """
#             Once we figure out cluster connection, this class will use the same
#             connection mechanism as `ClipperClusterManager`.
#         """
#         pass
#
#
#
#     def add_model(self, name, model_wrapper, model_data, rollout_strategy):
#         """
#             Add a new model to an existing model wrapper.
#
#             When the model wrapper docker container is launched, the special
#             environment variable `CLIPPER_MODEL_DATA` is set to the path
#             where the `model_data` package was unzipped.
#
#             Args:
#                 name(str): unique identifier for this model.                
#                 model_wrapper(str): path to Docker container
#                 model_data(str): path to file or root of a directory containing
#                                  model data (e.g. parameters). If the path contains a zipfile
#                                  or tarball the file will be shipped as is, otherwise a tarball
#                                  will be created. Either way, the file will be unpackaged at the destination.
#
#
#             Raises:
#                 ValueError: If the provided name conflicts with an existing model name.
#
#         """
#         pass
#
#     def update_model(self, name, model_data, rollout_strategy):
#         """
#             Update to a new version of an existing model.
#
#             It will launch a new replica of the model wrapper
#             (according to the update strategy) with CLIPPER_MODEL_DATA
#             pointing to the new data.
#
#             Question:
#                 How to specify partial updates?
#         """
#         pass
#
#     def rollback_model(self, name, version, rollout_strategy):
#         """
#             Rollback the named model to the specified version.
#             
#
#             Raises:
#                 ValueError: If the supplied version of the model does not exist.
#         """
#
#         pass
#
#     def replicate_model(self, name, num_replicas, nodes):
#         """
#             Replicate the specified model wrapper `num_replica` times on each of
#             the specified nodes.
#         """
#         pass
#
#
#     def alias(self, name, version, new_name, rollout_strategy):
#         """
#             Create a replica of an existing model wrapper with a new
#             name and treat it as a new model. From now on the two models
#             will be considered independent, and version updates to one will
#             not affect the other.
#         """
#         pass
#
#     def inspect_model(self, name):
#         """
#             Get diagnostic information about a model. This includes both
#             history of the model (who last modified it, when it was last updated)
#             but also performance (e.g. latency and throughput, cache hit rates,
#             how often it misses the latency SLO, etc.)
#         """
#         pass
#
#     def set_model_permissions(self, name, permissions):
#         """
#             Let's look into Etcd permissions for this.
#         """
#         pass
#
#
#
# class ClipperClusterManager:
#     """
#         All actions through the API are associated with a specific user
#         to track provenance and add permissions.
#
#
#
#
#         How does cluster membership work?
#         How does cluster deployment work?
#         Atomicity, Isolation, Idempotence
#
#         Proposal:
#             Let's use etcd for cluster membership.
#             Does that mean the cluster manager connects directly to Etcd
#             to make changes, or does it connect to one of the Clipper instances
#             which then propagates the changes to Etcd? How do we want to manage shipping
#             models which could be 100s of MB?
#
#     """
#
#     def __init__(user_id):
#         pass
#
#
#     
#     def start(num_instances, nodes):
#         """
#             Start a new Clipper cluster on the provided list of machines.
#             
#             Questions:
#             How to access these machines? SSH? Expect a cluster manager?
#
#         """
#         pass
#
#     def connect(address):
#         """
#             Connect to a running Clipper cluster.
#
#             Questions:
#             + How is cluster membership handled? Etcd?
#             
#         """
#         pass
#
#     def shutdown(self):
#         """
#             Shutdown the connected cluster
#
#         """
#         pass
#
#     def get_metrics(self):
#         self.get_system_metrics()
#         self.get_model_metrics()
#
#     def get_system_metrics(self):
#         """
#             Get physical performance metrics (latency, throughput, cache hits, perf, etc.)
#         """
#         pass
#
#     def get_model_metrics(self):
#         """
#             Get model performance metrics
#         """
#         pass
#
#
#
#
#
#
#
#
#
















