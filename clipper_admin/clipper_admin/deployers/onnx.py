from __future__ import print_function, with_statement, absolute_import
import shutil
import torch
import logging
import os
from warnings import warn

from ..version import __registry__, __version__
from .deployer_utils import save_python_function

logger = logging.getLogger(__name__)


def create_pytorch_endpoint(clipper_conn,
                            name,
                            input_type,
                            inputs,
                            func,
                            pytorch_model,
                            default_output="None",
                            version=1,
                            slo_micros=3000000,
                            labels=None,
                            registry=None,
                            base_image=None,
                            num_replicas=1,
                            onnx_backend="caffe2",
                            batch_size=-1,
                            pkgs_to_install=None):
    """This function deploys the prediction function with a PyTorch model.
    It serializes the PyTorch model in Onnx format and creates a container that loads it as a
    Caffe2 model.

    Parameters
    ----------
    clipper_conn : :py:meth:`clipper_admin.ClipperConnection`
        A ``ClipperConnection`` object connected to a running Clipper cluster.
    name : str
        The name to be assigned to both the registered application and deployed model.
    input_type : str
        The input_type to be associated with the registered app and deployed model.
        One of "integers", "floats", "doubles", "bytes", or "strings".
    inputs :
        input of func.
    func : function
        The prediction function. Any state associated with the function will be
        captured via closure capture and pickled with Cloudpickle.
    pytorch_model : pytorch model object
        The PyTorch model to save.
    default_output : str, optional
        The default output for the application. The default output will be returned whenever
        an application is unable to receive a response from a model within the specified
        query latency SLO (service level objective). The reason the default output was returned
        is always provided as part of the prediction response object. Defaults to "None".
    version : str, optional
        The version to assign this model. Versions must be unique on a per-model
        basis, but may be re-used across different models.
    slo_micros : int, optional
        The query latency objective for the application in microseconds.
        This is the processing latency between Clipper receiving a request
        and sending a response. It does not account for network latencies
        before a request is received or after a response is sent.
        If Clipper cannot process a query within the latency objective,
        the default output is returned. Therefore, it is recommended that
        the SLO not be set aggressively low unless absolutely necessary.
        100000 (100ms) is a good starting value, but the optimal latency objective
        will vary depending on the application.
    labels : list(str), optional
        A list of strings annotating the model. These are ignored by Clipper
        and used purely for user annotations.
    registry : str, optional
        The Docker container registry to push the freshly built model to. Note
        that if you are running Clipper on Kubernetes, this registry must be accesible
        to the Kubernetes cluster in order to fetch the container from the registry.
    base_image : str, optional
        The base Docker image to build the new model image from. This
        image should contain all code necessary to run a Clipper model
        container RPC client.
    num_replicas : int, optional
        The number of replicas of the model to create. The number of replicas
        for a model can be changed at any time with
        :py:meth:`clipper.ClipperConnection.set_num_replicas`.
    onnx_backend : str, optional
        The provided onnx backend.Caffe2 is the only currently supported ONNX backend.
    batch_size : int, optional
        The user-defined query batch size for the model. Replicas of the model will attempt
        to process at most `batch_size` queries simultaneously. They may process smaller
        batches if `batch_size` queries are not immediately available.
        If the default value of -1 is used, Clipper will adaptively calculate the batch size for
        individual replicas of this model.
    pkgs_to_install : list (of strings), optional
        A list of the names of packages to install, using pip, in the container.
        The names must be strings.
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_pytorch_model(clipper_conn, name, version, input_type, inputs, func,
                         pytorch_model, base_image, labels, registry,
                         num_replicas, onnx_backend, batch_size,
                         pkgs_to_install)

    clipper_conn.link_model_to_app(name, name)


def deploy_pytorch_model(clipper_conn,
                         name,
                         version,
                         input_type,
                         inputs,
                         func,
                         pytorch_model,
                         base_image=None,
                         labels=None,
                         registry=None,
                         num_replicas=1,
                         onnx_backend="caffe2",
                         batch_size=-1,
                         pkgs_to_install=None):
    """This function deploys the prediction function with a PyTorch model.
    It serializes the PyTorch model in Onnx format and creates a container that loads it as a
    Caffe2 model.
    
    Parameters
    ----------
    clipper_conn : :py:meth:`clipper_admin.ClipperConnection`
        A ``ClipperConnection`` object connected to a running Clipper cluster.
    name : str
        The name to be assigned to both the registered application and deployed model.
    version : str
        The version to assign this model. Versions must be unique on a per-model
        basis, but may be re-used across different models.
    input_type : str
        The input_type to be associated with the registered app and deployed model.
        One of "integers", "floats", "doubles", "bytes", or "strings".
    inputs :
        input of func.
    func : function
        The prediction function. Any state associated with the function will be
        captured via closure capture and pickled with Cloudpickle.
    pytorch_model : pytorch model object
        The Pytorch model to save.
    base_image : str, optional
        The base Docker image to build the new model image from. This
        image should contain all code necessary to run a Clipper model
        container RPC client.
    labels : list(str), optional
        A list of strings annotating the model. These are ignored by Clipper
        and used purely for user annotations.
    registry : str, optional
        The Docker container registry to push the freshly built model to. Note
        that if you are running Clipper on Kubernetes, this registry must be accesible
        to the Kubernetes cluster in order to fetch the container from the registry.
    num_replicas : int, optional
        The number of replicas of the model to create. The number of replicas
        for a model can be changed at any time with
        :py:meth:`clipper.ClipperConnection.set_num_replicas`.
    onnx_backend : str, optional
        The provided onnx backend.Caffe2 is the only currently supported ONNX backend.
    batch_size : int, optional
        The user-defined query batch size for the model. Replicas of the model will attempt
        to process at most `batch_size` queries simultaneously. They may process smaller
        batches if `batch_size` queries are not immediately available.
        If the default value of -1 is used, Clipper will adaptively calculate the batch size for
        individual replicas of this model.
    pkgs_to_install : list (of strings), optional
        A list of the names of packages to install, using pip, in the container.
        The names must be strings.
    """
    warn("""
The caffe 2 version is not up to date because
https://github.com/ucbrise/clipper/issues/475,
however you may still be able use it.
We will update our caffe2 build soon.""")

    if base_image is None:
        if onnx_backend is "caffe2":
            base_image = "{}/caffe2-onnx-container:{}".format(
                __registry__, __version__)
        else:
            logger.error(
                "{backend} ONNX backend is not currently supported.".format(
                    backend=onnx_backend))

    serialization_dir = save_python_function(name, func)

    try:
        torch.onnx._export(
            pytorch_model,
            inputs,
            os.path.join(serialization_dir, "model.onnx"),
            export_params=True)
        # Deploy model
        clipper_conn.build_and_deploy_model(
            name, version, input_type, serialization_dir, base_image, labels,
            registry, num_replicas, batch_size, pkgs_to_install)

    except Exception as e:
        logger.error(
            "Error serializing PyTorch model to ONNX: {e}".format(e=e))

    logger.info("Torch model has be serialized to ONNX format")

    # Remove temp files
    shutil.rmtree(serialization_dir)
