from __future__ import print_function, with_statement, absolute_import
import shutil
import pyspark
import logging
import re
import os
import json
import sys

from ..version import __version__, __registry__
from ..clipper_admin import ClipperException
from .deployer_utils import save_python_function

logger = logging.getLogger(__name__)


def create_endpoint(clipper_conn,
                    name,
                    input_type,
                    func,
                    pyspark_model,
                    sc,
                    default_output="None",
                    version=1,
                    slo_micros=3000000,
                    labels=None,
                    registry=None,
                    base_image="default",
                    num_replicas=1,
                    batch_size=-1,
                    pkgs_to_install=None):
    """Registers an app and deploys the provided predict function with PySpark model as
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
    pyspark_model : pyspark.mllib.* or pyspark.ml.pipeline.PipelineModel object
        The PySpark model to save.
    sc : SparkContext,
        The current SparkContext. This is needed to save the PySpark model.
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
        If the default value of -1 is used, Clipper will adaptively calculate the batch size for individual
        replicas of this model.
    pkgs_to_install : list (of strings), optional
        A list of the names of packages to install, using pip, in the container.
        The names must be strings.
    """

    clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
    deploy_pyspark_model(clipper_conn, name, version, input_type, func,
                         pyspark_model, sc, base_image, labels, registry,
                         num_replicas, batch_size, pkgs_to_install)

    clipper_conn.link_model_to_app(name, name)


def deploy_pyspark_model(clipper_conn,
                         name,
                         version,
                         input_type,
                         func,
                         pyspark_model,
                         sc,
                         base_image="default",
                         labels=None,
                         registry=None,
                         num_replicas=1,
                         batch_size=-1,
                         pkgs_to_install=None):
    """Deploy a Python function with a PySpark model.

    The function must take 3 arguments (in order): a SparkSession, the PySpark model, and a list of
    inputs. It must return a list of strings of the same length as the list of inputs.

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
    pyspark_model : pyspark.mllib.* or pyspark.ml.pipeline.PipelineModel object
        The PySpark model to save.
    sc : SparkContext,
        The current SparkContext. This is needed to save the PySpark model.
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
        If the default value of -1 is used, Clipper will adaptively calculate the batch size for individual
        replicas of this model.
    pkgs_to_install : list (of strings), optional
        A list of the names of packages to install, using pip, in the container.
        The names must be strings.

    Example
    -------
    Define a pre-processing function ``shift()`` to normalize prediction inputs::

        from clipper_admin import ClipperConnection, DockerContainerManager
        from clipper_admin.deployers.pyspark import deploy_pyspark_model
        from pyspark.mllib.classification import LogisticRegressionWithSGD
        from pyspark.sql import SparkSession
        import numpy as np

        spark = SparkSession.builder.appName("example").getOrCreate()

        sc = spark.sparkContext

        clipper_conn = ClipperConnection(DockerContainerManager())

        # Connect to an already-running Clipper cluster
        clipper_conn.connect()

        # Loading a training dataset omitted...
        model = LogisticRegressionWithSGD.train(trainRDD, iterations=10)

        def shift(x):
            return x - np.mean(x)


        # Note that this function accesses the trained PySpark model via an explicit
        # argument, but other state can be captured via closure capture if necessary.
        def predict(spark, model, inputs):
            return [str(model.predict(shift(x))) for x in inputs]

        deploy_pyspark_model(
            clipper_conn,
            name="example",
            input_type="doubles",
            func=predict,
            pyspark_model=model,
            sc=sc)
    """

    model_class = re.search("pyspark.*'",
                            str(type(pyspark_model))).group(0).strip("'")
    if model_class is None:
        raise ClipperException(
            "pyspark_model argument was not a pyspark object")

    # save predict function
    serialization_dir = save_python_function(name, func)
    # save Spark model
    spark_model_save_loc = os.path.join(serialization_dir,
                                        "pyspark_model_data")
    try:
        if isinstance(pyspark_model,
                      pyspark.ml.pipeline.PipelineModel) or isinstance(
                          pyspark_model, pyspark.ml.base.Model):
            pyspark_model.save(spark_model_save_loc)
        else:
            pyspark_model.save(sc, spark_model_save_loc)
    except Exception as e:
        logger.warning("Error saving spark model: %s" % e)
        raise e

    # extract the pyspark class name. This will be something like
    # pyspark.mllib.classification.LogisticRegressionModel
    with open(os.path.join(serialization_dir, "metadata.json"),
              "w") as metadata_file:
        json.dump({"model_class": model_class}, metadata_file)

    logger.info("Spark model saved")

    py_minor_version = (sys.version_info.major, sys.version_info.minor)
    # Check if Python 2 or Python 3 image
    if base_image == "default":
        if py_minor_version < (3, 0):
            logger.info("Using Python 2 base image")
            base_image = "{}/pyspark-container:{}".format(
                __registry__, __version__)
        elif py_minor_version == (3, 5):
            logger.info("Using Python 3.5 base image")
            base_image = "{}/pyspark35-container:{}".format(
                __registry__, __version__)
        elif py_minor_version == (3, 6):
            logger.info("Using Python 3.6 base image")
            base_image = "{}/pyspark36-container:{}".format(
                __registry__, __version__)
        elif py_minor_version == (3, 7):
            logger.info("Using Python 3.7 base image")
            base_image = "{}/pyspark37-container:{}".format(
                __registry__, __version__)
        else:
            msg = ("PySpark deployer only supports Python 2.7, 3.5, 3.6, and 3.7. "
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

    # Remove temp files
    shutil.rmtree(serialization_dir)
