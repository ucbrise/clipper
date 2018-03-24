from __future__ import print_function, with_statement, absolute_import
import shutil
import mxnet as mx
import logging
import re
import os
import json

from ..version import __version__
from ..clipper_admin import ClipperException
from .deployer_utils import save_python_function, serialize_object

logger = logging.getLogger(__name__)

MXNET_MODEL_RELATIVE_PATH = "mxnet_model"


def create_endpoint(
        clipper_conn,
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
        base_image="clipper/mxnet-container:{}".format(__version__),
        num_replicas=1):
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
    mxnet_data_shapes : list(int)
        List of integers representing the dimensions of data used for model prediction.
        Required because loading serialized MXNet models involves binding, which requires
        the shape of the data used to train the model.
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

    Note
    ----
    Regarding `mxnet_data_shapes` parameter:
    Clipper may provide the model with variable size input batches. Because MXNet can't
    handle variable size input batches, we recommend setting batch size for input data
    to 1, or dynamically reshaping the model with every prediction based on the current
    input batch size.
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_mxnet_model(clipper_conn, name, version, input_type, func,
                       mxnet_model, mxnet_data_shapes, base_image, labels,
                       registry, num_replicas)

    clipper_conn.link_model_to_app(name, name)


def deploy_mxnet_model(
        clipper_conn,
        name,
        version,
        input_type,
        func,
        mxnet_model,
        mxnet_data_shapes,
        base_image="clipper/mxnet-container:{}".format(__version__),
        labels=None,
        registry=None,
        num_replicas=1):
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
    mxnet_data_shapes : list(int)
        List of integers representing the dimensions of data used for model prediction.
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

    Note
    ----
    Regarding `mxnet_data_shapes` parameter:
    Clipper may provide the model with variable size input batches. Because MXNet can't
    handle variable size input batches, we recommend setting batch size for input data
    to 1, or dynamically reshaping the model with every prediction based on the current
    input batch size.

    Example
    -------

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

        # Initialize the module and fit it
        mxnet_model = mx.mod.Module(softmax)
        mxnet_model.fit(data_iter, num_epoch=1)

        data_shape = [1, 785]

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

        # Deploy model
        clipper_conn.build_and_deploy_model(name, version, input_type,
                                            serialization_dir, base_image,
                                            labels, registry, num_replicas)

    except Exception as e:
        logger.error("Error saving MXNet model: %s" % e)

    logger.info("MXNet model saved")

    # Remove temp files
    shutil.rmtree(serialization_dir)
