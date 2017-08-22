from __future__ import absolute_import, division, print_function
import logging
import docker
import requests
from requests.exceptions import RequestException
import json
import os
from .container_manager import CONTAINERLESS_MODEL_IMAGE
import time
import re

from .exceptions import ClipperException

from .version import __version__

DEFAULT_LABEL = ["DEFAULT"]

logger = logging.getLogger(__name__)




# deploy_regex_str = "[a-z0-9]([-a-z0-9]*[a-z0-9])?(\\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*\Z"
deploy_regex_str = "[a-z0-9]([-a-z0-9]*[a-z0-9])?\Z"
deployment_regex = re.compile(deploy_regex_str)

# NOTE: Label regex should match a superset of the strings that match deployment_regex
# label_regex = re.compile("(([A-Za-z0-9][-A-Za-z0-9_.]*)?[A-Za-z0-9])?\Z")


def validate_versioned_model_name(name, version):
    if deployment_regex.match(name) is None:
        raise ClipperException("Invalid value: {name}: a model name must be a valid DNS-1123 "
                               " subdomain. It must consist of lower case "
                               "alphanumeric characters, '-' or '.', and must start and end with "
                               "an alphanumeric character (e.g. 'example.com', regex used for "
                               "validation is '{reg}'"
                               .format(name=name, reg=deploy_regex_str))
    if deployment_regex.match(version) is None:
        raise ClipperException("Invalid value: {version}: a model version must be a valid DNS-1123 "
                               " subdomain. It must consist of lower case "
                               "alphanumeric characters, '-' or '.', and must start and end with "
                               "an alphanumeric character (e.g. 'example.com', regex used for "
                               "validation is '{reg}'"
                               .format(version=version, reg=deploy_regex_str))


class ClipperConnection(object):
    def __init__(self, container_manager):
        self.cm = container_manager

    def start_clipper(self,
                      query_frontend_image='clipper/query_frontend:{}'.format(__version__),
                      mgmt_frontend_image='clipper/management_frontend:{}'.format(__version__)):
        try:
            self.cm.start_clipper(query_frontend_image, mgmt_frontend_image)
            while True:
                try:
                    url = "http://{host}/metrics".format(host=self.cm.get_query_addr())
                    requests.get(url, timeout=5)
                    break
                except RequestException as e:
                    logger.info("Clipper still initializing.")
                    time.sleep(1)
            logger.info("Clipper is running")
            return True
        except ClipperException as e:
            logger.info(e.msg)
            return False

    def connect(self):
        self.cm.connect()

    def register_application(self, name, input_type, default_output,
                             slo_micros):
        url = "http://{host}/admin/add_app".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({
            "name": name,
            "input_type": input_type,
            "default_output": default_output,
            "latency_slo_micros": slo_micros
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def link_model_to_app(self, app_name, model_name):
        """
        Allows the model with `model_name` to be used by the app with `app_name`.
        The model and app should both be registered with Clipper.

        Parameters
        ----------
        app_name : str
            The name of the application
        model_name : str
            The name of the model to link to the application

        Returns
        -------
        bool
            Returns true iff the model link request was successful
        """

        url = "http://{host}/admin/add_model_links".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({
            "app_name": app_name,
            "model_names": [model_name]
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        logger.info(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def build_and_deploy_model(self,
                               name,
                               version,
                               input_type,
                               model_data_path,
                               base_image,
                               labels=None,
                               container_registry=None,
                               num_replicas=1):
        version = str(version)

        validate_versioned_model_name(name, version)

        with open(model_data_path + '/Dockerfile', 'w') as f:
            f.write("FROM {container_name}\nCOPY . /model/.\n".format(
                container_name=base_image))

        # build, tag, and push docker image to registry
        # NOTE: DOCKER_API_VERSION (set by `minikube docker-env`) must be same
        # version as docker registry server

        image = "{name}:{version}".format(name=name, version=version)
        if container_registry is not None:
            image = "{reg}/{image}".format(reg=container_registry, image=image)
        # if "DOCKER_API_VERSION" in os.environ:
        #     docker_client = docker.from_env(
        #         version=os.environ["DOCKER_API_VERSION"])
        # else:
        docker_client = docker.from_env()
        logger.info(
            "Building model Docker image with model data from {}".format(model_data_path))
        docker_client.images.build(path=model_data_path, tag=image)

        logger.info("Pushing model Docker image to {}".format(image))
        # if container_registry is not None:
        docker_client.images.push(repository=image)
        self.deploy_model(name, version, input_type, image, labels, num_replicas)

    def deploy_model(self,
                     name,
                     version,
                     input_type,
                     image,
                     labels=None,
                     num_replicas=1):

        validate_versioned_model_name(name, version)
        self.cm.deploy_model(
            name, version, input_type, image, num_replicas=num_replicas)
        logger.info("Registering model with Clipper")
        self.register_model(
            name, version, input_type, image=image, labels=labels)
        logger.info("Done deploying!")

    def register_model(self, name, version, input_type, image=None,
                       labels=None):
        version = str(version)
        url = "http://{host}/admin/add_model".format(
            host=self.cm.get_admin_addr())
        if image is None:
            image = CONTAINERLESS_MODEL_IMAGE
        if labels is None:
            labels = DEFAULT_LABEL
        req_json = json.dumps({
            "model_name": name,
            "model_version": version,
            "labels": labels,
            "input_type": input_type,
            "container_name": image,
            "model_data_path": "DEPRECATED",
        })
        headers = {'Content-type': 'application/json'}
        logger.debug(req_json)
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def get_num_replicas(self, name, version):
        if version is None:
            model_info = self.get_all_models(verbose=True)
            for m in model_info:
                if m["is_current_version"]:
                    version = m["model_version"]
        assert version is not None
        version = str(version)
        return self.cm.get_num_replicas(name, version)

    def set_num_replicas(self, name, version=None, num_replicas=1):
        """Set the total number of active replicas for a model.

        If there are more than the current number of replicas
        currently allocated, this will remove replicas. If there are
        less, this will add replicas.

        Parameters
        ----------
        name : str
            The name of the model
        version : str, optional
            The version of the model. If no version is provided,
            the currently deployed version will be used.
        num_replicas : int, optional
            The desired number of replicas.
        """
        if version is None:
            model_info = self.get_all_models(verbose=True)
            for m in model_info:
                if m["is_current_version"]:
                    version = m["model_version"]
        assert version is not None

        version = str(version)
        model_data = self.get_model_info(name, version)
        if model_data is not None:
            input_type = model_data["input_type"]
            image = model_data["container_name"]
            if image != CONTAINERLESS_MODEL_IMAGE:
                self.cm.set_num_replicas(name, version, input_type, image,
                                         num_replicas)
            else:
                msg = "Cannot resize the replica set for containerless model {name}:{version}".format(
                    name=name, version=version)
                logger.error(msg)
                raise ClipperException(msg)
        else:
            msg = "Cannot add container for non-registered model {name}:{version}".format(
                name=name, version=version)
            logger.error(msg)
            raise ClipperException(msg)

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
        url = "http://{host}/admin/get_all_applications".format(host=self.cm.get_admin_addr())
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

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
        url = "http://{host}/admin/get_application".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"name": name})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                return None
            return app_info
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def get_linked_models(self, app_name):
        """Retrieves the models linked to the specified application

        Parameters
        ----------
        app_name : str
            The name of the application

        Returns
        -------
        list
            Returns a list of the names of models linked to the app.
            If no models are linked to the specified app, None is returned.
        """
        url = "http://{host}/admin/get_linked_models".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"app_name": app_name})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

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
        url = "http://{host}/admin/get_all_models".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            logger.warning(r.text)
            return None

    def get_model_info(self, name, version):
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
        version = str(version)
        url = "http://{host}/admin/get_model".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"model_name": name, "model_version": version})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                return None
            return app_info
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def get_all_model_replicas(self, verbose=False):
        """Gets information about all model containers registered with Clipper.

        Parameters
        ----------
        verbose : bool
            If set to False, the returned list contains the apps' names.
            If set to True, the list contains container info dictionaries.

        Returns
        -------
        list
            Returns a list of information about all model containers known to Clipper.
            If no containers are registered with Clipper, an empty list is returned.
        """
        url = "http://{host}/admin/get_all_containers".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def get_model_replica_info(self, name, version, replica_id):
        """Gets detailed information about a registered container.

        Parameters
        ----------
        name : str
            The name of the container to look up
        version : int
            The version of the container to look up
        replica_id : int
            The container replica to look up

        Returns
        -------
        dict
            A dictionary with the specified container's info.
            If no corresponding container is registered with Clipper, None is returned.
        """
        version = str(version)
        url = "http://{host}/admin/get_container".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({
            "model_name": name,
            "model_version": version,
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
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def get_clipper_logs(self, logging_dir="clipper_logs/"):
        return self.cm.get_logs(logging_dir)

    def inspect_instance(self):
        """Fetches metrics from the running Clipper instance.

        Returns
        -------
        str
            The JSON string containing the current set of metrics
            for this instance. On error, the string will be an error message
            (not JSON formatted).
        """
        url = "http://{host}/metrics".format(host=self.cm.get_query_addr())
        r = requests.get(url)
        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

    def set_model_version(self, name, version, num_replicas=None):
        """Changes the current model version to `model_version`.

        This method can be used to do model rollback and rollforward to
        any previously deployed version of the model. Note that model
        versions automatically get updated when `deploy_model()` is
        called, so there is no need to manually update the version as well.

        Parameters
        ----------
        name : str
            The name of the model
        version : str | obj with __str__ representation
            The version of the model. Note that `version`
            must be a model version that has already been deployed.
        num_replicas : int
            The number of new containers to start with the newly
            selected model version.

        """
        version = str(version)
        url = "http://{host}/admin/set_model_version".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"model_name": name, "model_version": version})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)

        if num_replicas is not None:
            self.set_num_replicas(name, version, num_replicas)

    def stop_inactive_model_versions(self, model_name):
        """Removes all containers serving stale versions of the specified model.

        Parameters
        ----------
        model_name : str
            The name of the model whose old containers you want to clean.
        """
        model_info = self.get_all_models(verbose=True)
        current_version = None
        for m in model_info:
            if m["is_current_version"]:
                current_version = m["model_version"]
        assert current_version is not None
        self.cm.stop_models(
            model_name=model_name, keep_version=current_version)

    def get_query_addr(self):
        return self.cm.get_query_addr()

    def stop_deployed_models(self):
        self.cm.stop_models()

    def stop_clipper(self):
        self.cm.stop_clipper()

    def stop_all(self):
        self.cm.stop_models()
        self.cm.stop_clipper()
