from __future__ import print_function, with_statement, absolute_import

import logging
import shutil

from .deployer_utils import save_python_function

logger = logging.getLogger(__name__)


def create_endpoint(clipper_conn,
                    name,
                    input_type,
                    func,
                    default_output="None",
                    version=1,
                    slo_micros=100000,
                    labels=None,
                    registry=None,
                    base_image="clipper/python-closure-container",
                    num_replicas=1):
    """Registers an app and deploys provided predict function as a model.

    Parameters
    ----------
    name : str
        The to be assigned to the registered app and deployed model.
    predict_function : function
        The prediction function. Any state associated with the function should be
        captured via closure capture.
    input_type : str
        The input_type to be associated with the registered app and deployed model.
        One of "integers", "floats", "doubles", "bytes", or "strings".
    default_output : string, optional
        The default prediction to use if the model does not return a prediction
        by the end of the latency objective.
    model_version : Any object with a string representation (with __str__ implementation), optional
        The version to assign the deployed model.
    slo_micros : int
        The query latency objective for the application in microseconds.
        This is the processing latency between Clipper receiving a request
        and sending a response. It does not account for network latencies
        before a request is received or after a response is sent.
    labels : list of str, optional
        A list of strings annotating the model.
    num_containers : int, optional
        The number of replicas of the model to create. More replicas can be
        created later as well.
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_python_closure(clipper_conn, name, version, input_type, func,
                          base_image, labels, registry, num_replicas)

    clipper_conn.link_model_to_app(name, name)


def deploy_python_closure(clipper_conn,
                          name,
                          version,
                          input_type,
                          func,
                          base_image="clipper/python-closure-container",
                          labels=None,
                          registry=None,
                          num_replicas=1):
    # TODO: fix documentation
    """Deploy an arbitrary Python function to Clipper.

    The function should take a list of inputs of the type specified by `input_type` and
    return a Python or numpy array of predictions as strings. All dependencies for the function
    must be installed with Anaconda or Pip and this function must be called from within an Anaconda
    environment.

    Parameters
    ----------
    name : str
        The name to assign this model.
    version : int
        The version to assign this model.
    predict_function : function
        The prediction function. Any state associated with the function should be
        captured via closure capture.
    input_type : str
        One of "integers", "floats", "doubles", "bytes", or "strings".
    labels : list of str, optional
        A list of strings annotating the model
    num_containers : int, optional
        The number of replicas of the model to create. More replicas can be
        created later as well. Defaults to 1.

    Returns
    -------
    bool
        True if the model was successfully deployed. False otherwise.

    Example
    -------
    Define a feature function ``center()`` and train a model on the featurized input::

        from clipper_admin.deployers.python import deploy_python_closure
        def center(xs):
            means = np.mean(xs, axis=0)
            return xs - means

        centered_xs = center(xs)
        model = sklearn.linear_model.LogisticRegression()
        model.fit(centered_xs, ys)

        def centered_predict(inputs):
            centered_inputs = center(inputs)
            return model.predict(centered_inputs)

        deploy_python_closure(
            "example_model",
            1,
            centered_predict,
            "doubles",
            num_containers=1)
    """

    serialization_dir = save_python_function(name, func)
    logger.info("Python closure saved")
    # Deploy function
    deploy_result = clipper_conn.deploy_model(name, version, input_type,
                                              serialization_dir, base_image,
                                              labels, registry, num_replicas)
    # Remove temp files
    shutil.rmtree(serialization_dir)
    return deploy_result
