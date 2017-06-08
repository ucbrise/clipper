from __future__ import print_function, with_statement, absolute_import
import shutil
import pyspark
import logging
import re
import os
import json

from ..clipper_admin import ClipperException
from .deployer_utils import save_python_function
from ..clipper_admin import deploy_model

logger = logging.getLogger(__name__)


def deploy_pyspark_model(cm,
                         name,
                         version,
                         input_type,
                         func,
                         pyspark_model,
                         sc,
                         base_image="clipper/pyspark-container",
                         labels=None,
                         registry=None,
                         num_replicas=1):
    # TODO: fix documentation
    """Deploy a Spark MLLib model to Clipper.

    Parameters
    ----------
    name : str
        The name to assign this model.
    version : int
        The version to assign this model.
    predict_function : function
        A function that takes three arguments, a SparkContext, the ``model`` parameter and
        a list of inputs of the type specified by the ``input_type`` argument.
        Any state associated with the function other than the Spark model should
        be captured via closure capture. Note that the function must not capture
        the SparkContext or the model implicitly, as these objects are not pickleable
        and therefore will prevent the ``predict_function`` from being serialized.
    pyspark_model : pyspark.mllib.util.Saveable
        An object that mixes in the pyspark Saveable mixin. Generally this
        is either an mllib model or transformer. This model will be loaded
        into the Clipper model container and provided as an argument to the
        predict function each time it is called.
    sc : SparkContext
        The SparkContext associated with the model. This is needed
        to save the model for pyspark.mllib models.
    input_type : str
        One of "integers", "floats", "doubles", "bytes", or "strings".
    labels : list of str, optional
        A set of strings annotating the model
    num_containers : int, optional
        The number of replicas of the model to create. More replicas can be
        created later as well. Defaults to 1.

    Returns
    -------
    bool
        True if the model was successfully deployed. False otherwise.
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
        if isinstance(pyspark_model, pyspark.ml.pipeline.PipelineModel):
            pyspark_model.save(spark_model_save_loc)
        else:
            pyspark_model.save(sc, spark_model_save_loc)
    except Exception as e:
        logger.warn("Error saving spark model: %s" % e)
        raise e

    # extract the pyspark class name. This will be something like
    # pyspark.mllib.classification.LogisticRegressionModel
    with open(os.path.join(serialization_dir, "metadata.json"),
              "w") as metadata_file:
        json.dump({"model_class": model_class}, metadata_file)

    logger.info("Spark model saved")

    # Deploy model
    deploy_result = deploy_model(cm, name, version, input_type,
                                 serialization_dir, base_image, labels, registry, num_replicas)

    # Remove temp files
    shutil.rmtree(serialization_dir)

    return deploy_result
