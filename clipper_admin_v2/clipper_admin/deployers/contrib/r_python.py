from __future__ import print_function, with_statement, absolute_import
import shutil
import logging
import os
from rpy2.robjects.packages import importr
from ..clipper_admin import deploy_model

base = importr('base')

logger = logging.getLogger(__name__)

# TODO: Fix impl


def deploy_R_model(cm, name, version, model_data, labels=None, num_replicas=1):
    # TODO: fix documentation
    """Registers a model with Clipper and deploys instances of it in containers.
    Parameters
    ----------
    name : str
        The name to assign this model.
    version : int
        The version to assign this model.
    model_data :
        The trained model to add to Clipper.The type has to be rpy2.robjects.vectors.ListVector,
        this is how python's rpy2 encapsulates any given R model.This model will be loaded
        into the Clipper model container and provided as an argument to the
        predict function each time it is called.
    labels : list of str, optional
        A set of strings annotating the model
    num_containers : int, optional
        The number of replicas of the model to create. More replicas can be
        created later as well. Defaults to 1.
    """

    base_image = "clipper/r_python_container"
    input_type = "strings"
    fname = name.replace("/", "_")
    serialization_dir = os.path.join("/tmp", fname)
    rds_path = os.path.join(serialization_dir, "%s.rds" % fname)
    if not os.path.exists(serialization_dir):
        os.makedirs(serialization_dir)
    base.saveRDS(model_data, rds_path)

    logger.info("R model saved")

    deploy_result = deploy_model(cm, name, version, input_type,
                                 serialization_dir, base_image, labels,
                                 num_replicas)

    # Remove temp files
    shutil.rmtree(serialization_dir)

    return deploy_result
