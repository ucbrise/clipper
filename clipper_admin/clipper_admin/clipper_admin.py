from __future__ import absolute_import, division, print_function
import logging
import docker
from docker import errors
import tempfile
import requests
from urllib3.exceptions import TimeoutError
from requests.exceptions import (RequestException, Timeout)
import json
import pprint
import time
import re
import os
import tarfile
import sys
from cloudpickle import CloudPickler
import pickle
import numpy as np
if sys.version_info < (3, 0):
    try:
        from cStringIO import StringIO
    except ImportError:
        from StringIO import StringIO
    PY3 = False
else:
    from io import BytesIO as StringIO
    PY3 = True

from .decorators import retry
from .container_manager import CONTAINERLESS_MODEL_IMAGE, ClusterAdapter
from .exceptions import ClipperException, UnconnectedException
from .version import __version__, __registry__

DEFAULT_LABEL = []
DEFAULT_PREDICTION_CACHE_SIZE_BYTES = 33554432
CLIPPER_TEMP_DIR = "/tmp/clipper"  # Used Internally for Test; Not Windows Compatible

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

deploy_regex_str = "[a-z0-9]([-a-z0-9]*[a-z0-9])?\Z"
deployment_regex = re.compile(deploy_regex_str)


def _validate_versioned_model_name(name, version):
    if deployment_regex.match(name) is None:
        raise ClipperException(
            "Invalid value: {name}: a model name must be a valid DNS-1123 "
            " subdomain. It must consist of lower case "
            "alphanumeric characters, '-' or '.', and must start and end with "
            "an alphanumeric character (e.g. 'example.com', regex used for "
            "validation is '{reg}'".format(name=name, reg=deploy_regex_str))
    if deployment_regex.match(version) is None:
        raise ClipperException(
            "Invalid value: {version}: a model version must be a valid DNS-1123 "
            " subdomain. It must consist of lower case "
            "alphanumeric characters, '-' or '.', and must start and end with "
            "an alphanumeric character (e.g. 'example.com', regex used for "
            "validation is '{reg}'".format(
                version=version, reg=deploy_regex_str))


class ClipperConnection(object):
    def __init__(self, container_manager):
        """Create a new ClipperConnection object.

        After creating a ``ClipperConnection`` instance, you still need to connect
        to a Clipper cluster. You can connect to an existing cluster by calling
        :py:meth:`clipper_admin.ClipperConnection.connect` or create a new Clipper cluster
        with :py:meth:`clipper_admin.ClipperConnection.start_clipper`, which will automatically
        connect to the cluster once it Clipper has successfully started.

        Parameters
        ----------
        container_manager : ``clipper_admin.container_manager.ContainerManager``
            An instance of a concrete subclass of ``ContainerManager``.
        """
        self.connected = False
        self.cm = container_manager

        self.logger = ClusterAdapter(logger, {
            'cluster_name': self.cm.cluster_identifier
        })

    def start_clipper(self,
                      query_frontend_image='{}/query_frontend:{}'.format(
                          __registry__, __version__),
                      mgmt_frontend_image='{}/management_frontend:{}'.format(
                          __registry__, __version__),
                      frontend_exporter_image='{}/frontend-exporter:{}'.format(
                          __registry__, __version__),
                      cache_size=DEFAULT_PREDICTION_CACHE_SIZE_BYTES,
                      qf_http_thread_pool_size=1,
                      qf_http_timeout_request=5,
                      qf_http_timeout_content=300,
                      num_frontend_replicas=1):
        """Start a new Clipper cluster and connect to it.

        This command will start a new Clipper instance using the container manager provided when
        the ``ClipperConnection`` instance was constructed.

        Parameters
        ----------
        query_frontend_image : str(optional)
            The query frontend docker image to use. You can set this argument to specify
            a custom build of the query frontend, but any customization should maintain API
            compability and preserve the expected behavior of the system.
        mgmt_frontend_image : str(optional)
            The management frontend docker image to use. You can set this argument to specify
            a custom build of the management frontend, but any customization should maintain API
            compability and preserve the expected behavior of the system.
        frontend_exporter_image : str(optional)
            The frontend exporter docker image to use. You can set this argument to specify
            a custom build of the management frontend, but any customization should maintain API
            compability and preserve the expected behavior of the system.
        cache_size : int(optional)
            The size of Clipper's prediction cache in bytes. Default cache size is 32 MiB.
        qf_http_thread_pool_size : int(optional)
            The size of thread pool created in query frontend for http serving.
        qf_http_timeout_request : int(optional)
            The seconds of timeout on request handling in query frontend for http serving..
        qf_http_timeout_content : int(optional)
            The seconds of timeout on content handling in query frontend for http serving..
        num_frontend_replicas : int(optional)
            The number of query frontend to deploy for fault tolerance and high availability.

        Raises
        ------
        :py:exc:`clipper.ClipperException`
        """
        try:
            self.cm.start_clipper(query_frontend_image, mgmt_frontend_image,
                                  frontend_exporter_image, cache_size,
                                  qf_http_thread_pool_size, qf_http_timeout_request,
                                  qf_http_timeout_content, num_frontend_replicas)
        except ClipperException as e:
            self.logger.warning("Error starting Clipper: {}".format(e.msg))
            raise e

        # Wait for maximum 5 min.
        @retry(RequestException, tries=300, delay=1, backoff=1, logger=self.logger)
        def _check_clipper_status():
            try:
                query_frontend_url = "http://{host}/metrics".format(
                    host=self.cm.get_query_addr())
                mgmt_frontend_url = "http://{host}/admin/ping".format(
                    host=self.cm.get_admin_addr())
                for name, url in [('query frontend', query_frontend_url),
                                  ('management frontend', mgmt_frontend_url)]:
                    r = requests.get(url, timeout=5)
                    if r.status_code != requests.codes.ok:
                        raise RequestException(
                            "{name} end point {url} health check failed".format(name=name, url=url))
            except RequestException as e:
                raise RequestException("Clipper still initializing: \n {}".format(e))
        _check_clipper_status()

        self.logger.info("Clipper is running")
        self.connected = True

    def connect(self):
        """Connect to a running Clipper cluster."""

        self.cm.connect()
        self.connected = True
        self.logger.info(
            "Successfully connected to Clipper cluster at {}".format(
                self.cm.get_query_addr()))

    def register_application(self, name, input_type, default_output,
                             slo_micros):
        # TODO(crankshaw): Add links to user guide section on input types once user guide is
        # written:
        # "See the `User Guide <http://clipper.ai/user_guide/#input-types>`_ for more details
        # on picking the right input type for your application."
        """Register a new application with Clipper.

        An application in Clipper corresponds to a named REST endpoint that can be used to request
        predictions. This command will attempt to create a new endpoint with the provided name.
        Application names must be unique. This command will fail if an application with the provided
        name already exists.

        Parameters
        ----------
        name : str
            The unique name of the application.
        input_type : str
            The type of the request data this endpoint can process. Input type can be
            one of "integers", "floats", "doubles", "bytes", or "strings".
        default_output : str
            The default output for the application. The default output will be returned whenever
            an application is unable to receive a response from a model within the specified
            query latency SLO (service level objective). The reason the default output was returned
            is always provided as part of the prediction response object.
        slo_micros : int
            The query latency objective for the application in microseconds.
            This is the processing latency between Clipper receiving a request
            and sending a response. It does not account for network latencies
            before a request is received or after a response is sent.
            If Clipper cannot process a query within the latency objective,
            the default output is returned. Therefore, it is recommended that
            the SLO not be set aggressively low unless absolutely necessary.
            100000 (100ms) is a good starting value, but the optimal latency objective
            will vary depending on the application.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
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
        self.logger.debug(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)
        else:
            self.logger.info(
                "Application {app} was successfully registered".format(
                    app=name))

    def delete_application(self, name):
        # See: https://github.com/ucbrise/clipper/issues/603
        self.logger.warning("[DEPRECATED] Use 'unregister_application' API instead of this.")
        self.unregister_application(name)

    def unregister_application(self, name):
        if not self.connected:
            raise UnconnectedException()

        url = "http://{host}/admin/delete_app".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"name": name})
        headers = {"Content-type": "application/json"}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)
        else:
            self.logger.info(
                "Application {app} was successfully unregistered".format(app=name))

    def link_model_to_app(self, app_name, model_name):
        """Routes requests from the specified app to be evaluted by the specified model.

        Parameters
        ----------
        app_name : str
            The name of the application
        model_name : str
            The name of the model to link to the application

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`

        Note
        -----
        Both the specified model and application must be registered with Clipper, and they
        must have the same input type. If the application has previously been linked to a different
        model, this command will fail.
        """
        if not self.connected:
            raise UnconnectedException()

        url = "http://{host}/admin/add_model_links".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({
            "app_name": app_name,
            "model_names": [model_name]
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)
        else:
            self.logger.info(
                "Model {model} is now linked to application {app}".format(
                    model=model_name, app=app_name))

    def unlink_model_from_app(self, app_name, model_name):
        """
        Prevents the model with `model_name` from being used by the app with `app_name`.
        The model and app should both be registered with Clipper and a link should
        already exist between them.

        Parameters
        ----------
        app_name : str
            The name of the application
        model_name : str
            The name of the model to link to the application

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """

        if not self.connected:
            raise UnconnectedException()

        url = "http://{host}/admin/delete_model_links".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({
            "app_name": app_name,
            "model_names": [model_name]
        })
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        logger.debug(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            logger.error(msg)
            raise ClipperException(msg)
        else:
            logger.info(
                "Model {model} is now removed to application {app}".format(
                    model=model_name, app=app_name))

    def build_and_deploy_model(self,
                               name,
                               version,
                               input_type,
                               model_data_path,
                               base_image,
                               labels=None,
                               container_registry=None,
                               num_replicas=1,
                               batch_size=-1,
                               pkgs_to_install=None):
        """Build a new model container Docker image with the provided data and deploy it as
        a model to Clipper.

        This method does two things.

        1. Builds a new Docker image from the provided base image with the local directory specified
        by ``model_data_path`` copied into the image by calling
        :py:meth:`clipper_admin.ClipperConnection.build_model`.

        2. Registers and deploys a model with the specified metadata using the newly built
        image by calling :py:meth:`clipper_admin.ClipperConnection.deploy_model`.

        Parameters
        ----------
        name : str
            The name of the deployed model
        version : str
            The version to assign this model. Versions must be unique on a per-model
            basis, but may be re-used across different models.
        input_type : str
            The type of the request data this endpoint can process. Input type can be
            one of "integers", "floats", "doubles", "bytes", or "strings". See the
            `User Guide <http://clipper.ai/user_guide/#input-types>`_ for more details
            on picking the right input type for your application.
        model_data_path : str
            A path to a local directory. The contents of this directory will be recursively copied
            into the Docker container.
        base_image : str
            The base Docker image to build the new model image from. This
            image should contain all code necessary to run a Clipper model
            container RPC client.
        labels : list(str), optional
            A list of strings annotating the model. These are ignored by Clipper
            and used purely for user annotations.
        container_registry : str, optional
            The Docker container registry to push the freshly built model to. Note
            that if you are running Clipper on Kubernetes, this registry must be accesible
            to the Kubernetes cluster in order to fetch the container from the registry.
        num_replicas : int, optional
            The number of replicas of the model to create. The number of replicas
            for a model can be changed at any time with
            :py:meth:`clipper.ClipperConnection.set_num_replicas`.
        batch_size : int, optional
            The user-defined query batch size for the model. Replicas of the model will attempt
            to process at most `batch_size` queries simultaneously. They may process smaller
            batches if `batch_size` queries are not immediately available.
            If the default value of -1 is used, Clipper will adaptively calculate the batch size for
            individual replicas of this model.
        pkgs_to_install : list (of strings), optional
            A list of the names of packages to install, using pip, in the container.
            The names must be strings.
        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """

        if not self.connected:
            raise UnconnectedException()
        image = self.build_model(name, version, model_data_path, base_image,
                                 container_registry, pkgs_to_install)
        self.deploy_model(name, version, input_type, image, labels,
                          num_replicas, batch_size)

    def build_model(self,
                    name,
                    version,
                    model_data_path,
                    base_image,
                    container_registry=None,
                    pkgs_to_install=None):
        """Build a new model container Docker image with the provided data"

        This method builds a new Docker image from the provided base image with the local directory
        specified by ``model_data_path`` copied into the image. The Dockerfile that gets generated
        to build the image is equivalent to the following::

            FROM <base_image>
            COPY <model_data_path> /model/

        The newly built image is then pushed to the specified container registry. If no container
        registry is specified, the image will be pushed to the default DockerHub registry. Clipper
        will tag the newly built image with the tag [<registry>]/<name>:<version>.

        This method can be called without being connected to a Clipper cluster.

        Parameters
        ----------
        name : str
            The name of the deployed model.
        version : str
            The version to assign this model. Versions must be unique on a per-model
            basis, but may be re-used across different models.
        model_data_path : str
            A path to a local directory. The contents of this directory will be recursively copied
            into the Docker container.
        base_image : str
            The base Docker image to build the new model image from. This
            image should contain all code necessary to run a Clipper model
            container RPC client.
        container_registry : str, optional
            The Docker container registry to push the freshly built model to. Note
            that if you are running Clipper on Kubernetes, this registry must be accesible
            to the Kubernetes cluster in order to fetch the container from the registry.
        pkgs_to_install : list (of strings), optional
            A list of the names of packages to install, using pip, in the container.
            The names must be strings.
        Returns
        -------
        str :
            The fully specified tag of the newly built image. This will include the
            container registry if specified.

        Raises
        ------
        :py:exc:`clipper.ClipperException`

        Note
        ----
        Both the model name and version must be valid DNS-1123 subdomains. Each must consist of
        lower case alphanumeric characters, '-' or '.', and must start and end with an alphanumeric
        character (e.g. 'example.com', regex used for validation is
        '[a-z0-9]([-a-z0-9]*[a-z0-9])?\Z'.
        """

        version = str(version)

        _validate_versioned_model_name(name, version)

        run_cmd = ''
        if pkgs_to_install:
            run_as_lst = 'RUN apt-get -y install build-essential && pip install'.split(
                ' ')
            run_cmd = ' '.join(run_as_lst + pkgs_to_install)
        with tempfile.NamedTemporaryFile(
                mode="w+b", suffix="tar") as context_file:
            # Create build context tarfile
            with tarfile.TarFile(
                    fileobj=context_file, mode="w") as context_tar:
                context_tar.add(model_data_path)
                # From https://stackoverflow.com/a/740854/814642
                try:
                    df_contents = StringIO(
                        str.encode(
                            "FROM {container_name}\n{run_command}\nCOPY {data_path} /model/\n".
                            format(
                                container_name=base_image,
                                data_path=model_data_path,
                                run_command=run_cmd)))
                    df_tarinfo = tarfile.TarInfo('Dockerfile')
                    df_contents.seek(0, os.SEEK_END)
                    df_tarinfo.size = df_contents.tell()
                    df_contents.seek(0)
                    context_tar.addfile(df_tarinfo, df_contents)
                except TypeError:
                    df_contents = StringIO(
                        "FROM {container_name}\n{run_command}\nCOPY {data_path} /model/\n".
                        format(
                            container_name=base_image,
                            data_path=model_data_path,
                            run_command=run_cmd))
                    df_tarinfo = tarfile.TarInfo('Dockerfile')
                    df_contents.seek(0, os.SEEK_END)
                    df_tarinfo.size = df_contents.tell()
                    df_contents.seek(0)
                    context_tar.addfile(df_tarinfo, df_contents)
            # Exit Tarfile context manager to finish the tar file
            # Seek back to beginning of file for reading
            context_file.seek(0)
            image = "{cluster}-{name}:{version}".format(
                cluster=self.cm.cluster_identifier, name=name, version=version)
            if container_registry is not None:
                image = "{reg}/{image}".format(
                    reg=container_registry, image=image)
            docker_client = docker.from_env()
            self.logger.info(
                "Building model Docker image with model data from {}".format(
                    model_data_path))
            image_result, build_logs = docker_client.images.build(
                fileobj=context_file, custom_context=True, tag=image)
            for b in build_logs:
                if 'stream' in b and b['stream'] != '\n':  #log build steps only
                    self.logger.info(b['stream'].rstrip())

        self.logger.info("Pushing model Docker image to {}".format(image))

        @retry((docker.errors.APIError, TimeoutError, Timeout),
               tries=5, logger=self.logger)
        def _push_model():
            for line in docker_client.images.push(repository=image, stream=True):
                self.logger.debug(line)
        _push_model()

        return image

    def deploy_model(self,
                     name,
                     version,
                     input_type,
                     image,
                     labels=None,
                     num_replicas=1,
                     batch_size=-1):
        """Deploys the model in the provided Docker image to Clipper.

        Deploying a model to Clipper does a few things.

        1. It starts a set of Docker model containers running the model packaged
        in the ``image`` Docker image. The number of containers it will start is dictated
        by the ``num_replicas`` argument, but the way that these containers get started
        depends on your choice of ``ContainerManager`` implementation.

        2. It registers the model and version with Clipper and sets the current version of the
        model to this version by internally calling
        :py:meth:`clipper_admin.ClipperConnection.register_model`.

        Notes
        -----
        If you want to deploy a model in some other way (e.g. a model that cannot run in a Docker
        container for some reason), you can start the model manually or with an external tool and
        call ``register_model`` directly.

        Parameters
        ----------
        name : str
            The name of the deployed model
        version : str
            The version to assign this model. Versions must be unique on a per-model
            basis, but may be re-used across different models.
        input_type : str
            The type of the request data this endpoint can process. Input type can be
            one of "integers", "floats", "doubles", "bytes", or "strings". See the
            `User Guide <http://clipper.ai/user_guide/#input-types>`_ for more details
            on picking the right input type for your application.
        image : str
             The fully specified Docker image to deploy. If using a custom
             registry, the registry name must be prepended to the image. For example,
             if your Docker image is stored in the quay.io registry, you should specify
             the image argument as
             "quay.io/my_namespace/image_name:tag". The image name and tag are independent of
             the ``name`` and ``version`` arguments, and can be set to whatever you want.
        labels : list(str), optional
            A list of strings annotating the model. These are ignored by Clipper
            and used purely for user annotations.
        num_replicas : int, optional
            The number of replicas of the model to create. The number of replicas
            for a model can be changed at any time with
            :py:meth:`clipper.ClipperConnection.set_num_replicas`.
        batch_size : int, optional
            The user-defined query batch size for the model. Replicas of the model will attempt
            to process at most `batch_size` queries simultaneously. They may process smaller
            batches if `batch_size` queries are not immediately available.
            If the default value of -1 is used, Clipper will adaptively calculate the batch size for
            individual replicas of this model.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`

        Note
        ----
        Both the model name and version must be valid DNS-1123 subdomains. Each must consist of
        lower case alphanumeric characters, '-' or '.', and must start and end with an alphanumeric
        character (e.g. 'example.com', regex used for validation is
        '[a-z0-9]([-a-z0-9]*[a-z0-9])?\Z'.
        """
        if not self.connected:
            raise UnconnectedException()
        version = str(version)
        _validate_versioned_model_name(name, version)
        self.cm.deploy_model(
            name=name,
            version=version,
            input_type=input_type,
            image=image,
            num_replicas=num_replicas)
        self.register_model(
            name,
            version,
            input_type,
            image=image,
            labels=labels,
            batch_size=batch_size)
        self.logger.info("Done deploying model {name}:{version}.".format(
            name=name, version=version))

    def register_model(self,
                       name,
                       version,
                       input_type,
                       image=None,
                       labels=None,
                       batch_size=-1):
        """Registers a new model version with Clipper.

        This method does not launch any model containers, it only registers the model description
        (metadata such as name, version, and input type) with Clipper. A model must be registered
        with Clipper before it can be linked to an application.

        You should rarely have to use this method directly. Using one the Clipper deployer
        methods in :py:mod:`clipper_admin.deployers` or calling ``build_and_deploy_model`` or
        ``deploy_model`` will automatically register your model with Clipper.

        Parameters
        ----------
        name : str
            The name of the deployed model
        version : str
            The version to assign this model. Versions must be unique on a per-model
            basis, but may be re-used across different models.
        input_type : str
            The type of the request data this endpoint can process. Input type can be
            one of "integers", "floats", "doubles", "bytes", or "strings". See the
            `User Guide <http://clipper.ai/user_guide/#input-types>`_ for more details
            on picking the right input type for your application.
        image : str, optional
            A docker image name. If provided, the image will be recorded as part of the
            model descrtipin in Clipper when registering the model but this method will
            make no attempt to launch any containers with this image.
        labels : list(str), optional
            A list of strings annotating the model. These are ignored by Clipper
            and used purely for user annotations.
        batch_size : int, optional
            The user-defined query batch size for the model. Replicas of the model will attempt
            to process at most `batch_size` queries simultaneously. They may process smaller
            batches if `batch_size` queries are not immediately available.
            If the default value of -1 is used, Clipper will adaptively calculate the batch size for
            individual replicas of this model.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """

        if not self.connected:
            raise UnconnectedException()
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
            "batch_size": batch_size
        })

        headers = {'Content-type': 'application/json'}
        self.logger.debug(req_json)
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)
        else:
            self.logger.info(
                "Successfully registered model {name}:{version}".format(
                    name=name, version=version))

    def get_current_model_version(self, name):
        """Get the current model version for the specified model.

        Parameters
        ----------
        name : str
            The name of the model

        Returns
        -------
        str
            The current model version

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`

        """
        if not self.connected:
            raise UnconnectedException()
        version = None
        model_info = self.get_all_models(verbose=True)
        for m in model_info:
            if m["model_name"] == name and m["is_current_version"]:
                version = m["model_version"]
                break
        if version is None:
            raise ClipperException(
                "No versions of model {} registered with Clipper".format(name))
        return version

    def get_num_replicas(self, name, version=None):
        """Gets the current number of model container replicas for a model.

        Parameters
        ----------
        name : str
            The name of the model
        version : str, optional
            The version of the model. If no version is provided,
            the currently deployed version will be used.

        Returns
        -------
        int
            The number of active replicas

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
        if version is None:
            version = self.get_current_model_version(name)
        else:
            version = str(version)
        return self.cm.get_num_replicas(name, version)

    def set_num_replicas(self, name, num_replicas, version=None):
        """Sets the total number of active replicas for a model.

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

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
        if version is None:
            version = self.get_current_model_version(name)
        else:
            version = str(version)
        model_data = self.get_model_info(name, version)
        if model_data is not None:
            input_type = model_data["input_type"]
            image = model_data["container_name"]
            if image != CONTAINERLESS_MODEL_IMAGE:
                self.cm.set_num_replicas(name, version, input_type, image,
                                         num_replicas)
            else:
                msg = ("Cannot resize the replica set for containerless model "
                       "{name}:{version}").format(
                           name=name, version=version)
                self.logger.error(msg)
                raise ClipperException(msg)
        else:
            msg = "Cannot add container for non-registered model {name}:{version}".format(
                name=name, version=version)
            self.logger.error(msg)
            raise ClipperException(msg)

    def get_all_apps(self, verbose=False):
        """Gets information about all applications registered with Clipper.

        Parameters
        ----------
        verbose : bool
            If set to False, the returned list contains the apps' names.
            If set to True, the list contains application info dictionaries.
            These dictionaries have the same attribute name-value pairs that were
            provided to :py:meth:`clipper_admin.ClipperConnection.register_application`.

        Returns
        -------
        list
            Returns a list of information about all apps registered to Clipper.
            If no apps are registered with Clipper, an empty list is returned.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """

        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/admin/get_all_applications".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
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
            :py:meth:`clipper_admin.ClipperConnection.register_application`.
            If no application with name ``name`` is
            registered with Clipper, None is returned.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        """
        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/admin/get_application".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"name": name})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)

        if r.status_code == requests.codes.ok:
            app_info = r.json()
            if len(app_info) == 0:
                self.logger.warning(
                    "Application {} is not registered with Clipper".format(
                        name))
                return None
            return app_info
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)

    def get_linked_models(self, app_name):
        """Retrieves the models linked to the specified application.

        Parameters
        ----------
        app_name : str
            The name of the application

        Returns
        -------
        list
            Returns a list of the names of models linked to the app.
            If no models are linked to the specified app, empty list is returned.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """

        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/admin/get_linked_models".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"app_name": app_name})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)
        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
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

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/admin/get_all_models".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)

        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)

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

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
        version = str(version)
        url = "http://{host}/admin/get_model".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"model_name": name, "model_version": version})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)

        if r.status_code == requests.codes.ok:
            model_info = r.json()
            if len(model_info) == 0:
                self.logger.warning(
                    "Model {name}:{version} is not registered with Clipper.".
                    format(name=name, version=version))
                return None
            return model_info
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
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

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/admin/get_all_containers".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"verbose": verbose})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)
        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
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

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
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
        self.logger.debug(r.text)

        if r.status_code == requests.codes.ok:
            model_rep_info = r.json()
            if len(model_rep_info) == 0:
                self.logger.warning(
                    "No model replica with ID {rep_id} found for model {name}:{version}".
                    format(rep_id=replica_id, name=name, version=version))
                return None
            return model_rep_info
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)

    def get_clipper_logs(self, logging_dir="clipper_logs/"):
        """Download the logs from all Clipper docker containers.

        Parameters
        ----------
        logging_dir : str, optional
            The directory to save the downloaded logs. If the directory does not
            exist, it will be created.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`

        """
        if not self.connected:
            raise UnconnectedException()
        return self.cm.get_logs(logging_dir)

    def inspect_instance(self):
        """Fetches performance metrics from the running Clipper cluster.

        Returns
        -------
        str
            The JSON string containing the current set of metrics
            for this instance. On error, the string will be an error message
            (not JSON formatted).

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`
        """
        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/metrics".format(host=self.cm.get_query_addr())
        r = requests.get(url)
        self.logger.debug(r.text)
        if r.status_code == requests.codes.ok:
            return r.json()
        else:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)

    def set_model_version(self, name, version, num_replicas=None):
        """Changes the current model version to "model_version".

        This method can be used to perform model roll-back and roll-forward. The
        version can be set to any previously deployed version of the model.

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

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        :py:exc:`clipper.ClipperException`

        Note
        -----
        Model versions automatically get updated when
        py:meth:`clipper_admin.ClipperConnection.deploy_model()` is called. There is no need to
        manually update the version after deploying a new model.
        """
        if not self.connected:
            raise UnconnectedException()
        version = str(version)
        url = "http://{host}/admin/set_model_version".format(
            host=self.cm.get_admin_addr())
        req_json = json.dumps({"model_name": name, "model_version": version})
        headers = {'Content-type': 'application/json'}
        r = requests.post(url, headers=headers, data=req_json)
        self.logger.debug(r.text)
        if r.status_code != requests.codes.ok:
            msg = "Received error status code: {code} and message: {msg}".format(
                code=r.status_code, msg=r.text)
            self.logger.error(msg)
            raise ClipperException(msg)

        if num_replicas is not None:
            self.set_num_replicas(name, num_replicas, version)

    def get_query_addr(self):
        """Get the IP address at which the query frontend can be reached request predictions.

        Returns
        -------
        str
            The address as an IP address or hostname.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
            versions. All replicas for each version of each model will be stopped.
        """

        if not self.connected:
            raise UnconnectedException()
        return self.cm.get_query_addr()

    def get_metric_addr(self):
        """Get the IP address of Prometheus metric server.
        Returns
        -------
        str
            The address as an IP address or hostname.
        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
            versions. All replicas for each version of each model will be stopped.
        """

        if not self.connected:
            raise UnconnectedException()
        return self.cm.get_metric_addr()

    def _unregister_versioned_models(self, model_versions_dict):
        """Unregister the specified versions of the specified models from Clipper internal.

        This function does not be opened to public because it might cause critical operation.
        Please use 'stop_models', 'stop_versioned_models', 'stop_inactive_model_versions',
        and 'stop_all_model_containers' APIs according to your need.

        Parameters
        ----------
        model_versions_dict : dict(str, list(str))
            For each entry in the dict, the key is a model name and the value is a list of model
        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
            versions. All replicas for each version of each model will be stopped.
        """
        if not self.connected:
            raise UnconnectedException()
        url = "http://{host}/admin/delete_versioned_model".format(
            host=self.cm.get_admin_addr())
        headers = {"Content-type": "application/json"}
        for model_name in model_versions_dict:
            for model_version in model_versions_dict[model_name]:
                req_json = json.dumps({"model_name": model_name,
                                       "model_version": model_version})
                r = requests.post(url, headers=headers, data=req_json)
                logger.debug(r.text)
                if r.status_code != requests.codes.ok:
                    msg = "Received error status code: {code} and message: " \
                          "{msg}".format(code=r.status_code, msg=r.text)
                    logger.error(msg)
                    raise ClipperException(msg)
                else:
                    logger.info(
                        "Model {name}:{ver} was successfully deleted".format(
                            name=model_name, ver=model_version))

    def stop_models(self, model_names):
        """Stops all versions of the specified models.

        This is a convenience method to avoid the need to explicitly list all versions
        of a model when calling :py:meth:`clipper_admin.ClipperConnection.stop_versioned_models`.

        Parameters
        ----------
        model_names : list(str)
            A list of model names. All replicas of all versions of each model specified in the list
            will be stopped.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
            versions. All replicas for each version of each model will be stopped.
        """
        if not self.connected:
            raise UnconnectedException()
        model_info = self.get_all_models(verbose=True)
        model_dict = {}
        for m in model_info:
            if m["model_name"] in model_names:
                if m["model_name"] in model_dict:
                    model_dict[m["model_name"]].append(m["model_version"])
                else:
                    model_dict[m["model_name"]] = [m["model_version"]]
        self.cm.stop_models(model_dict)
        self._unregister_versioned_models(model_dict)
        pp = pprint.PrettyPrinter(indent=4)
        self.logger.info(
            "Stopped all containers for these models and versions:\n{}".format(
                pp.pformat(model_dict)))

    def stop_versioned_models(self, model_versions_dict):
        """Stops the specified versions of the specified models.

        Parameters
        ----------
        model_versions_dict : dict(str, list(str))
            For each entry in the dict, the key is a model name and the value is a list of model

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
            versions. All replicas for each version of each model will be stopped.

        Note
        ----
        This method will stop the currently deployed versions of models if you specify them. You
        almost certainly want to use one of the other stop_* methods. Use with caution.
        """
        if not self.connected:
            raise UnconnectedException()
        self.cm.stop_models(model_versions_dict)
        self._unregister_versioned_models(model_versions_dict)
        pp = pprint.PrettyPrinter(indent=4)
        self.logger.info(
            "Stopped all containers for these models and versions:\n{}".format(
                pp.pformat(model_versions_dict)))

    def stop_inactive_model_versions(self, model_names):
        """Stops all model containers serving stale versions of the specified models.

        For example, if you have deployed versions 1, 2, and 3 of model "music_recommender"
        and version 3 is the current version::

            clipper_conn.stop_inactive_model_versions(["music_recommender"])

        will stop any containers serving versions 1 and 2 but will leave containers serving
        version 3 untouched.

        Parameters
        ----------
        model_names : list(str)
            The names of the models whose old containers you want to stop.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
        """
        if not self.connected:
            raise UnconnectedException()
        model_info = self.get_all_models(verbose=True)
        model_dict = {}
        for m in model_info:
            if m["model_name"] in model_names and not m["is_current_version"]:
                if m["model_name"] in model_dict:
                    model_dict[m["model_name"]].append(m["model_version"])
                else:
                    model_dict[m["model_name"]] = [m["model_version"]]
        self.cm.stop_models(model_dict)
        self._unregister_versioned_models(model_dict)
        pp = pprint.PrettyPrinter(indent=4)
        self.logger.info(
            "Stopped all containers for these models and versions:\n{}".format(
                pp.pformat(model_dict)))

    def stop_all_model_containers(self):
        """Stops all model containers started via Clipper admin commands.

        This method can be used to clean up leftover Clipper model containers even if the
        Clipper management frontend or Redis has crashed. It can also be called without calling
        ``connect`` first.

        Raises
        ------
        :py:exc:`clipper.UnconnectedException`
            versions. All replicas for each version of each model will be stopped.
        """
        if not self.connected:
            raise UnconnectedException()
        model_info = self.get_all_models(verbose=True)
        model_dict = {}
        for m in model_info:
            if m["model_name"] in model_dict:
                model_dict[m["model_name"]].append(m["model_version"])
            else:
                model_dict[m["model_name"]] = [m["model_version"]]
        self.cm.stop_all_model_containers()
        self._unregister_versioned_models(model_dict)
        pp = pprint.PrettyPrinter(indent=4)
        self.logger.info("Stopped all Clipper model containers:\n{}".format(
            pp.pformat(model_dict)))

    def stop_all(self, graceful=True):
        """Stops all processes that were started via Clipper admin commands.

        This includes the query and management frontend Docker containers and all model containers.
        If you started Redis independently, this will not affect Redis. It can also be called
        without calling ``connect`` first.

        If graceful=False, Clipper will issue Docker Kill if it's in the Docker Mode. This parameter
        will take not effect in Kubernetes.
        """
        self.cm.stop_all(graceful=graceful)
        self.logger.info(
            "Stopped all Clipper cluster and all model containers")

    def test_predict_function(self, query, func, input_type):
        """Tests that the user's function has the correct signature and can be properly saved and
        loaded.

        The function should take a dict request object like the query frontend expects JSON,
        the predict function, and the input type for the model.

        For example, the function can be called like:
            clipper_conn.test_predict_function({"input": [1.0, 2.0, 3.0]}, predict_func, "doubles")

        Parameters
        ----------
        query: JSON or list of dicts
            Inputs to test the prediction function on.
        func: function
            Predict function to test.
        input_type: str
            The input_type to be associated with the registered app and deployed model.
            One of "integers", "floats", "doubles", "bytes", or "strings".
        """
        if not self.connected:
            self.connect()
        query_data = list(x for x in list(query.values()))
        query_key = list(query.keys())

        if query_key[0] == "input_batch":
            query_data = query_data[0]

        try:
            flattened_data = [
                item for sublist in query_data for item in sublist
            ]
        except TypeError:
            return "Invalid input type or JSON key"

        numpy_data = None

        if input_type == "bytes":
            numpy_data = list(np.int8(x) for x in query_data)
            for x in flattened_data:
                if type(x) != bytes:
                    return "Invalid input type"

        if input_type == "integers":
            numpy_data = list(np.int32(x) for x in query_data)
            for x in flattened_data:
                if type(x) != int:
                    return "Invalid input type"

        if input_type == "floats" or input_type == "doubles":
            if input_type == "floats":
                numpy_data = list(np.float32(x) for x in query_data)
            else:
                numpy_data = list(np.float64(x) for x in query_data)
            for x in flattened_data:
                if type(x) != float:
                    return "Invalid input type"

        if input_type == "string":
            numpy_data = list(np.str_(x) for x in query_data)
            for x in flattened_data:
                if type(x) != str:
                    return "Invalid input type"

        s = StringIO()
        c = CloudPickler(s, 2)
        c.dump(func)
        serialized_func = s.getvalue()
        reloaded_func = pickle.loads(serialized_func)

        try:
            assert reloaded_func
        except AssertionError:
            self.logger.error(
                "Function does not properly serialize and reload")
            return "Function does not properly serialize and reload"

        return reloaded_func(numpy_data)
