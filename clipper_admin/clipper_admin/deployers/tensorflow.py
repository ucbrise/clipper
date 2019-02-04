from __future__ import print_function, with_statement, absolute_import
import shutil
import tensorflow as tf
import logging
import os
import glob
import sys

from ..version import __version__, __registry__
from .deployer_utils import save_python_function
from ..exceptions import ClipperException
from tensorflow import Session

logger = logging.getLogger(__name__)


def create_endpoint(clipper_conn,
                    name,
                    input_type,
                    func,
                    tf_sess_or_saved_model_path,
                    default_output="None",
                    version=1,
                    slo_micros=3000000,
                    labels=None,
                    registry=None,
                    base_image="default",
                    num_replicas=1,
                    batch_size=-1,
                    pkgs_to_install=None):
    """Registers an app and deploys the provided predict function with TensorFlow model as
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
    tf_sess : tensorflow.python.client.session.Session
        The Tensorflow Session to save or path to an existing saved model.
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
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_tensorflow_model(clipper_conn, name, version, input_type, func,
                            tf_sess_or_saved_model_path, base_image, labels,
                            registry, num_replicas, batch_size,
                            pkgs_to_install)

    clipper_conn.link_model_to_app(name, name)


def deploy_tensorflow_model(clipper_conn,
                            name,
                            version,
                            input_type,
                            func,
                            tf_sess_or_saved_model_path,
                            base_image="default",
                            labels=None,
                            registry=None,
                            num_replicas=1,
                            batch_size=-1,
                            pkgs_to_install=None):
    """Deploy a Python prediction function with a Tensorflow session or saved Tensorflow model.
    
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
    tf_sess : tensorflow.python.client.session.Session
        The tensor flow session to save or path to an existing saved model.
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

    Example
    -------
    Save and deploy a tensorflow session::

        from clipper_admin import ClipperConnection, DockerContainerManager
        from clipper_admin.deployers.tensorflow import deploy_tensorflow_model

        clipper_conn = ClipperConnection(DockerContainerManager())

        # Connect to an already-running Clipper cluster
        clipper_conn.connect()

        def predict(sess, inputs):
            preds = sess.run('predict_class:0', feed_dict={'pixels:0': inputs})
            return [str(p) for p in preds]

        deploy_tensorflow_model(
            clipper_conn,
            model_name,
            version,
            input_type,
            predict_fn,
            sess)

    """
    # save predict function
    serialization_dir = save_python_function(name, func)
    # save Tensorflow session or copy the saved model into the image
    if isinstance(tf_sess_or_saved_model_path, Session):
        tf_sess_save_loc = os.path.join(serialization_dir,
                                        "tfmodel/model.ckpt")
        try:
            saver = tf.train.Saver()
            save_path = saver.save(tf_sess_or_saved_model_path,
                                   tf_sess_save_loc)
        except Exception as e:
            logger.warning("Error saving Tensorflow model: %s" % e)
            raise e
        logger.info("TensorFlow model saved at: %s " % save_path)
    else:
        # Check if its a frozen Graph or a saved tensorflow Model

        # A typical Tensorflow model contains 4 files:
        #  model-ckpt.meta: This contains the complete graph.
        #                   [This contains a serialized MetaGraphDef protocol buffer.
        #  model-ckpt.data-0000-of-00001: This contains all the values of variables(weights, biases,
        #                                 placeholders,gradients, hyper-parameters etc).
        #  model-ckpt.index: metadata.
        #  checkpoint: All checkpoint information

        # Frozen Graph
        # Single encapsulated file(.pb extension) without un-necessary meta-data, gradients and
        # un-necessary training variables

        if os.path.isdir(tf_sess_or_saved_model_path):
            # Directory - Check for Frozen Graph or a Saved Tensorflow Model
            is_frozen_graph = glob.glob(
                os.path.join(tf_sess_or_saved_model_path, "*.pb"))
            if (len(is_frozen_graph) > 0):
                try:
                    shutil.copytree(tf_sess_or_saved_model_path,
                                    os.path.join(serialization_dir, "tfmodel"))
                except Exception as e:
                    logger.error(
                        "Error copying Frozen Tensorflow model: %s" % e)
                    raise e
            else:
                # Check if all the 4 files are present
                suffixes = [os.path.join(tf_sess_or_saved_model_path, suffix) \
                           for suffix in ("*.meta", "*.index", "checkpoint", "*.data*")]
                if sum([1 for suffix in suffixes if glob.glob(suffix)]) == 4:
                    try:
                        shutil.copytree(tf_sess_or_saved_model_path,
                                        os.path.join(serialization_dir,
                                                     "tfmodel"))
                    except Exception as e:
                        logger.error("Error copying Tensorflow model: %s" % e)
                        raise e
                else:
                    logger.error(
                        "Tensorflow Model: %s not found or some files are missing"
                        % tf_sess_or_saved_model_path)
                    raise ClipperException(
                        "Frozen Tensorflow Model: %s not found or some files are missing "
                        % tf_sess_or_saved_model_path)
        else:
            # File provided ...check if file exists and a frozen model
            # Check if a frozen model exists or else error out
            if (os.path.isfile(tf_sess_or_saved_model_path)
                    and tf_sess_or_saved_model_path.lower().endswith(('.pb'))):
                os.makedirs(os.path.join(serialization_dir, "tfmodel"))
                try:
                    shutil.copyfile(
                        tf_sess_or_saved_model_path,
                        os.path.join(
                            serialization_dir, "tfmodel/" +
                            os.path.basename(tf_sess_or_saved_model_path)))
                except Exception as e:
                    logger.error(
                        "Error copying Frozen Tensorflow model: %s" % e)
                    raise e
            else:
                logger.error("Tensorflow Model: %s not found" %
                             tf_sess_or_saved_model_path)
                raise ClipperException("Frozen Tensorflow Model: %s not found "
                                       % tf_sess_or_saved_model_path)
        logger.info("TensorFlow model copied to: tfmodel ")

    py_minor_version = (sys.version_info.major, sys.version_info.minor)
    # Check if Python 2 or Python 3 image
    if base_image == "default":
        if py_minor_version < (3, 0):
            logger.info("Using Python 2 base image")
            base_image = "{}/tf-container:{}".format(__registry__, __version__)
        elif py_minor_version == (3, 5):
            logger.info("Using Python 3.5 base image")
            base_image = "{}/tf35-container:{}".format(__registry__,
                                                       __version__)
        elif py_minor_version == (3, 6):
            logger.info("Using Python 3.6 base image")
            base_image = "{}/tf36-container:{}".format(__registry__,
                                                       __version__)
        else:
            msg = (
                "TensorFlow deployer only supports Python 2.7, 3.5, and 3.6. "
                "Detected {major}.{minor}").format(
                    major=sys.version_info.major, minor=sys.version_info.minor)
            logger.error(msg)
            # Remove temp files
            shutil.rmtree(serialization_dir)
            raise ClipperException(msg)

    # Deploy model
    clipper_conn.build_and_deploy_model(
        name, version, input_type, serialization_dir, base_image, labels,
        registry, num_replicas, batch_size, pkgs_to_install)

    # Remove temp files
    shutil.rmtree(serialization_dir)
