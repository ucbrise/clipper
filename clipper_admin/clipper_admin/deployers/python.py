from __future__ import print_function, with_statement, absolute_import

import logging
import shutil
from ..version import __version__

from .deployer_utils import save_python_function

logger = logging.getLogger(__name__)


def create_endpoint(
        clipper_conn,
        name,
        input_type,
        func,
        default_output="None",
        version=1,
        slo_micros=3000000,
        labels=None,
        registry=None,
        base_image="clipper/python-closure-container:{}".format(__version__),
        num_replicas=1):
    """Registers an application and deploys the provided predict function as a model.

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
        that if you are running Clipper on Kubernetes, this registry must be accessible
        to the Kubernetes cluster in order to fetch the container from the registry.
    base_image : str, optional
        The base Docker image to build the new model image from. This
        image should contain all code necessary to run a Clipper model
        container RPC client.
    num_replicas : int, optional
        The number of replicas of the model to create. The number of replicas
        for a model can be changed at any time with
        :py:meth:`clipper.ClipperConnection.set_num_replicas`.
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_python_closure(clipper_conn, name, version, input_type, func,
                          base_image, labels, registry, num_replicas)

    clipper_conn.link_model_to_app(name, name)


def deploy_python_closure(
        clipper_conn,
        name,
        version,
        input_type,
        func,
        base_image="clipper/python-closure-container:{}".format(__version__),
        labels=None,
        registry=None,
        num_replicas=1):
    """Deploy an arbitrary Python function to Clipper.

    The function should take a list of inputs of the type specified by `input_type` and
    return a Python list or numpy array of predictions as strings.

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


    Example
    -------
    Define a pre-processing function ``center()`` and train a model on the pre-processed input::

        from clipper_admin import ClipperConnection, DockerContainerManager
        from clipper_admin.deployers.python import deploy_python_closure
        import numpy as np
        import sklearn

        clipper_conn = ClipperConnection(DockerContainerManager())

        # Connect to an already-running Clipper cluster
        clipper_conn.connect()

        def center(xs):
            means = np.mean(xs, axis=0)
            return xs - means

        centered_xs = center(xs)
        model = sklearn.linear_model.LogisticRegression()
        model.fit(centered_xs, ys)

        # Note that this function accesses the trained model via closure capture,
        # rather than having the model passed in as an explicit argument.
        def centered_predict(inputs):
            centered_inputs = center(inputs)
            # model.predict returns a list of predictions
            preds = model.predict(centered_inputs)
            return [str(p) for p in preds]

        deploy_python_closure(
            clipper_conn,
            name="example",
            input_type="doubles",
            func=centered_predict)
    """

    serialization_dir = save_python_function(name, func)
    logger.info("Python closure saved")
    # Deploy function
    clipper_conn.build_and_deploy_model(name, version, input_type,
                                        serialization_dir, base_image, labels,
                                        registry, num_replicas)
    # Remove temp files
    shutil.rmtree(serialization_dir)
