from __future__ import print_function, with_statement, absolute_import
import shutil
import logging
import os
import json
import sys

from ..version import __version__, __registry__
from ..exceptions import ClipperException
from .deployer_utils import save_python_function

logger = logging.getLogger(__name__)

MXNET_MODEL_RELATIVE_PATH = "mxnet_model"


def create_endpoint(clipper_conn,
                    name,
                    input_type,
                    func,
                    mxnet_model,
                    mxnet_data_shapes,
                    default_output="None",
                    version=1,
                    slo_micros=3000000,
                    labels=None,
                    registry=None,
                    base_image="default",
                    num_replicas=1,
                    batch_size=-1,
                    pkgs_to_install=None):
    """Registers an app and deploys the provided predict function with MXNet model as
    a Clipper model.

    Parameters
    ----------
    clipper_conn : :py:meth:`clipper_admin.ClipperConnection`
        A ``ClipperConnection`` object connected to a running Clipper cluster.
    name : str
        The name to be assigned to both the registered application and deployed model.
    input_type : str
        The input_type to be associated with the registered app and deployed model.
        One of "integers", "floats", "doubles", "bytes", or "strings".
    func : function
        The prediction function. Any state associated with the function will be
        captured via closure capture and pickled with Cloudpickle.
    mxnet_model : mxnet model object
        The MXNet model to save.
        the shape of the data used to train the model.
    mxnet_data_shapes : list of DataDesc objects
        List of DataDesc objects representing the name, shape, type and layout information
        of data used for model prediction.
        Required because loading serialized MXNet models involves binding, which requires
        https://mxnet.incubator.apache.org/api/python/module.html#mxnet.module.BaseModule.bind
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
    batch_size : int, optional
        The user-defined query batch size for the model. Replicas of the model will attempt
        to process at most `batch_size` queries simultaneously. They may process smaller
        batches if `batch_size` queries are not immediately available.
        If the default value of -1 is used, Clipper will adaptively calculate the batch size for
        individual replicas of this model.
    pkgs_to_install : list (of strings), optional
        A list of the names of packages to install, using pip, in the container.
        The names must be strings.

    Note
    ----
    Regarding `mxnet_data_shapes` parameter:
        Clipper may provide the model with variable size input batches. Because MXNet can't
        handle variable size input batches, we recommend setting batch size for input data
        to 1, or dynamically reshaping the model with every prediction based on the current
        input batch size.
        More information regarding a DataDesc object can be found here:
        https://mxnet.incubator.apache.org/versions/0.11.0/api/python/io.html#mxnet.io.DataDesc
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_mxnet_model(clipper_conn, name, version, input_type, func,
                       mxnet_model, mxnet_data_shapes, base_image, labels,
                       registry, num_replicas, batch_size, pkgs_to_install)

    clipper_conn.link_model_to_app(name, name)


def deploy_mxnet_model(clipper_conn,
                       name,
                       version,
                       input_type,
                       func,
                       mxnet_model,
                       mxnet_data_shapes,
                       base_image="default",
                       labels=None,
                       registry=None,
                       num_replicas=1,
                       batch_size=-1,
                       pkgs_to_install=None):
    """Deploy a Python function with a MXNet model.

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
    func : function
        The prediction function. Any state associated with the function will be
        captured via closure capture and pickled with Cloudpickle.
    mxnet_model : mxnet model object
        The MXNet model to save.
    mxnet_data_shapes : list of DataDesc objects
        List of DataDesc objects representing the name, shape, type and layout information
        of data used for model prediction.
        Required because loading serialized MXNet models involves binding, which requires
        the shape of the data used to train the model.
        https://mxnet.incubator.apache.org/api/python/module.html#mxnet.module.BaseModule.bind
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
    batch_size : int, optional
        The user-defined query batch size for the model. Replicas of the model will attempt
        to process at most `batch_size` queries simultaneously. They may process smaller
        batches if `batch_size` queries are not immediately available.
        If the default value of -1 is used, Clipper will adaptively calculate the batch size for
        individual replicas of this model.
    pkgs_to_install : list (of strings), optional
        A list of the names of packages to install, using pip, in the container.
        The names must be strings.

    Note
    ----
    Regarding `mxnet_data_shapes` parameter:
    Clipper may provide the model with variable size input batches. Because MXNet can't
    handle variable size input batches, we recommend setting batch size for input data
    to 1, or dynamically reshaping the model with every prediction based on the current
    input batch size.
    More information regarding a DataDesc object can be found here:
    https://mxnet.incubator.apache.org/versions/0.11.0/api/python/io.html#mxnet.io.DataDesc

    Example
    -------
    Create a MXNet model and then deploy it::
    
        from clipper_admin import ClipperConnection, DockerContainerManager
        from clipper_admin.deployers.mxnet import deploy_mxnet_model
        import mxnet as mx

        clipper_conn = ClipperConnection(DockerContainerManager())

        # Connect to an already-running Clipper cluster
        clipper_conn.connect()

        # Create a MXNet model
        # Configure a two layer neuralnetwork
        data = mx.symbol.Variable('data')
        fc1 = mx.symbol.FullyConnected(data, name='fc1', num_hidden=128)
        act1 = mx.symbol.Activation(fc1, name='relu1', act_type='relu')
        fc2 = mx.symbol.FullyConnected(act1, name='fc2', num_hidden=10)
        softmax = mx.symbol.SoftmaxOutput(fc2, name='softmax')

        # Load some training data
        data_iter = mx.io.CSVIter(
            data_csv="/path/to/train_data.csv", data_shape=(785, ), batch_size=1)

        # Initialize the module and fit it
        mxnet_model = mx.mod.Module(softmax)
        mxnet_model.fit(data_iter, num_epoch=1)

        data_shape = data_iter.provide_data

        deploy_mxnet_model(
            clipper_conn,
            name="example",
            version = 1,
            input_type="doubles",
            func=predict,
            mxnet_model=model,
            mxnet_data_shapes=data_shape)
    """

    serialization_dir = save_python_function(name, func)

    # save MXNet model
    mxnet_model_save_loc = os.path.join(serialization_dir,
                                        MXNET_MODEL_RELATIVE_PATH)

    try:

        # Saves model in two files: <serialization_dir>/mxnet_model.json will be saved for symbol,
        # <serialization_dir>/mxnet_model.params will be saved for parameters.
        mxnet_model.save_checkpoint(prefix=mxnet_model_save_loc, epoch=0)

        # Saves data_shapes to mxnet_model_metadata.json
        with open(
                os.path.join(serialization_dir, "mxnet_model_metadata.json"),
                "w") as f:
            json.dump({"data_shapes": mxnet_data_shapes}, f)

        logger.info("MXNet model saved")

        py_minor_version = (sys.version_info.major, sys.version_info.minor)
        # Check if Python 2 or Python 3 image
        if base_image == "default":
            if py_minor_version < (3, 0):
                logger.info("Using Python 2 base image")
                base_image = "{}/mxnet-container:{}".format(
                    __registry__, __version__)
            elif py_minor_version == (3, 5):
                logger.info("Using Python 3.5 base image")
                base_image = "{}/mxnet35-container:{}".format(
                    __registry__, __version__)
            elif py_minor_version == (3, 6):
                logger.info("Using Python 3.6 base image")
                base_image = "{}/mxnet36-container:{}".format(
                    __registry__, __version__)
            elif py_minor_version == (3, 7):
                logger.info("Using Python 3.7 base image")
                base_image = "{}/mxnet37-container:{}".format(
                    __registry__, __version__)
            else:
                msg = (
                    "MXNet deployer only supports Python 2.7, 3.5, 3.6, and 3.7. "
                    "Detected {major}.{minor}").format(
                        major=sys.version_info.major,
                        minor=sys.version_info.minor)
                logger.error(msg)
                # Remove temp files
                shutil.rmtree(serialization_dir)
                raise ClipperException(msg)

        # Deploy model
        clipper_conn.build_and_deploy_model(
            name, version, input_type, serialization_dir, base_image, labels,
            registry, num_replicas, batch_size, pkgs_to_install)

    except Exception as e:
        logger.error("Error saving MXNet model: %s" % e)
        raise e

    # Remove temp files
    shutil.rmtree(serialization_dir)
