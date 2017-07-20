from __future__ import absolute_import, division, print_function
import logging
import docker
import requests
import json
import os
from .container_manager import CONTAINERLESS_MODEL_IMAGE

DEFAULT_LABEL = ["DEFAULT"]

logger = logging.getLogger(__name__)


class ClipperException(Exception):
    pass


def start_clipper(cm):
    try:
        cm.start_clipper()
        logger.info("Clipper is running")
        return True
    except ClipperException as e:
        logger.info(e.msg)
        return False


def register_application(cm, name, model_name, input_type, default_output, slo_micros):
    url = "http://{host}/admin/add_app".format(host=cm.get_admin_addr())
    req_json = json.dumps({
        "name": name,
        "input_type": input_type,
        "candidate_model_names": [model_name],
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

def link_model_to_app(cm, app_name, model_name):
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

    url = "http://{host}/admin/add_model_links".format(host=cm.get_admin_addr())
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


def _resolve_docker_repo(cm, image, registry):
    if registry is None:
        registry = cm.get_registry()
    if registry is None:
        repo = image
    else:
        repo = "{registry}/{image}".format(
                registry=registry,
                image=image)
    return repo


def deploy_model(
        cm,
        name,
        version,
        input_type,
        model_data_path,
        base_image,
        labels=None,
        registry=None,
        num_replicas=1):
    version = str(version)

    with open(model_data_path + '/Dockerfile', 'w') as f:
        f.write("FROM {container_name}\nCOPY . /model/.\n".format(container_name=base_image))

    # build, tag, and push docker image to registry
    # NOTE: DOCKER_API_VERSION (set by `minikube docker-env`) must be same
    # version as docker registry server

    image_name = "{name}:{version}".format(
                name=name,
                version=version)
    repo = _resolve_docker_repo(cm, image_name, registry)
    if "DOCKER_API_VERSION" in os.environ:
        docker_client = docker.from_env(
            version=os.environ["DOCKER_API_VERSION"])
    else:
        docker_client = docker.from_env()
    logger.info("Building model Docker image at {}".format(model_data_path))
    docker_client.images.build(
            path=model_data_path,
            tag=repo)

    logger.info("Pushing model Docker image to {}".format(repo))
    cm.deploy_model(name, version, input_type, repo, num_replicas=num_replicas)
    logger.info("Registering model with Clipper")
    register_model(cm, name, version, input_type, repo=repo, labels=labels)
    logger.info("Done deploying!")


def register_model(cm, name, version, input_type, repo=None, labels=None):
    version = str(version)
    url = "http://{host}/admin/add_model".format(host=cm.get_admin_addr())
    if repo is None:
        repo = CONTAINERLESS_MODEL_IMAGE
    if labels is None:
        labels = DEFAULT_LABEL
    req_json = json.dumps({
        "model_name": name,
        "model_version": version,
        "labels": labels,
        "input_type": input_type,
        "container_name": repo,
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




def add_replica(cm, name, version):
    version = str(version)
    model_data = get_model_info(cm, name, version)
    if model_data is not None:
        input_type = model_data["input_type"]
        image = model_data["container_name"]
        if image != CONTAINERLESS_MODEL_IMAGE:
            cm.add_replica(name, version, input_type, image)
        else:
            msg = "Cannot add container for containerless model {name}:{version}".format(
                name=name, version=version)
            logger.error(msg)
            raise ClipperException(msg)
    else:
        msg = "Cannot add container for non-registered model {name}:{version}".format(
            name=name, version=version)
        logger.error(msg)
        raise ClipperException(msg)


def get_all_apps(cm, verbose=False):
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
    url = "http://{host}/admin/get_all_applications".format(host=cm.get_admin_addr())
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


def get_app_info(cm, name):
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
    url = "http://{host}/admin/get_application".format(host=cm.get_admin_addr())
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

def get_linked_models(cm, app_name):
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
    url = "http://{host}/admin/get_linked_models".format(host=cm.get_admin_addr())
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

def get_all_models(cm, verbose=False):
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
    url = "http://{host}/admin/get_all_models".format(host=cm.get_admin_addr())
    req_json = json.dumps({"verbose": verbose})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)

    if r.status_code == requests.codes.ok:
        return r.json()
    else:
        logger.warning(r.text)
        return None


def get_model_info(cm, name, version):
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
    url = "http://{host}/admin/get_model".format(host=cm.get_admin_addr())
    req_json = json.dumps({
        "model_name": name,
        "model_version": version
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


def get_all_model_replicas(cm, verbose=False):
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
    url = "http://{host}/admin/get_all_containers".format(host=cm.get_admin_addr())
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


def get_model_replica_info(cm, name, version, replica_id):
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
    url = "http://{host}/admin/get_container".format(host=cm.get_admin_addr())
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


def get_clipper_logs(cm, logging_dir="clipper_logs/"):
    return cm.get_logs(logging_dir)


def inspect_instance(cm):
    """Fetches metrics from the running Clipper instance.

    Returns
    -------
    str
        The JSON string containing the current set of metrics
        for this instance. On error, the string will be an error message
        (not JSON formatted).
    """
    url = "http://{host}/metrics".format(host=cm.get_query_addr())
    r = requests.get(url)
    if r.status_code == requests.codes.ok:
        return r.json()
    else:
        msg = "Received error status code: {code} and message: {msg}".format(
            code=r.status_code, msg=r.text)
        logger.error(msg)
        raise ClipperException(msg)


def set_model_version(cm, name, version, num_containers=0):
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
    num_containers : int
        The number of new containers to start with the newly
        selected model version.

    """
    version = str(version)
    url = "http://{host}/admin/set_model_version".format(host=cm.get_admin_addr())
    req_json = json.dumps({
        "model_name": name,
        "model_version": version
    })
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)
    if r.status_code != requests.codes.ok:
        msg = "Received error status code: {code} and message: {msg}".format(
            code=r.status_code, msg=r.text)
        logger.error(msg)
        raise ClipperException(msg)
    for r in range(num_containers):
        add_replica(cm, name, version)


def stop_inactive_model_versions(cm, model_name):
    """Removes all containers serving stale versions of the specified model.

    Parameters
    ----------
    model_name : str
        The name of the model whose old containers you want to clean.
    """
    model_info = get_all_models(cm, verbose=True)
    current_version = None
    for m in model_info:
        if m["is_current_version"]:
            current_version = m["model_version"]
    assert current_version is not None
    cm.stop_models(model_name=model_name, keep_version=current_version)


def stop_deployed_models(cm):
    cm.stop_models()


def stop_clipper(cm):
    cm.stop_clipper()


def stop_all(cm):
    cm.stop_models()
    cm.stop_clipper()
