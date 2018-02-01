from __future__ import print_function, with_statement, absolute_import
import shutil
import xgboost as xgb
import logging
import os

from ..version import __version__
from ..clipper_admin import ClipperException
from .deployer_utils import save_python_function

logger = logging.getLogger(__name__)


def create_endpoint(
         clipper_conn,
         name,
         input_type,
         func,
         xgboost_model,
         default_output="None",
         version=1,
         slo_micros=3000000,
         labels=None,
         registry=None,
         base_image="clipper/xgboost-container:{}".format(__version__),
         num_replicas=1,
         batch_size=-1):
    """Registers an app and deploys the provided predict function with XGBoost model as
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
        captured via closure capture and pickled with pickle.
        xgboost_model : xgboost.Booster object
        The XGBoost model to save.
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
        """
    
        clipper_conn.register_application(name, input_type, default_output,
                                      slo_micros)
                                      deploy_xgboost_model(clipper_conn, name, version, input_type, func,
                                                           base_image, labels, registry,
                                                           num_replicas, batch_size)
                                      
        clipper_conn.link_model_to_app(name, name)


def deploy_xgboost_model(
        clipper_conn,
        name,
        version,
        input_type,
        func,
        pyspark_model,
        sc,
        base_image="clipper/pyspark-container:{}".format(__version__),
        labels=None,
        registry=None,
        num_replicas=1,
        batch_size=-1):
    """Deploy a Python function with a XGBoost model.
    
    The function must take 1 argument: data in the form of a DMatrix. It should also be able to support optional parameters: output_margin (bool),
    ntree_limit (int), pred_leaf (bool), pred_contribs (bool), approx_contribs (bool). It must return an np array.
    
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
    captured via closure capture and pickled with pickle.
    xgboost_model : xgboost.Booster object
    The XGBoost model to save.
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
    
    Example
    -------
    Define a pre-processing function ``shift()`` and to normalize prediction inputs::
    
    from clipper_admin import ClipperConnection, DockerContainerManager
    from clipper_admin.deployers.xgboost import deploy_xgboost_model
    import xgboost as xgb
    
    clipper_conn = ClipperConnection(DockerContainerManager())
    
    # Connect to an already-running Clipper cluster
    clipper_conn.connect()
    
    # Loading a training dataset omitted...
    # specify parameters via map, definition are same as c++ version
    param = {'max_depth':2, 'eta':1, 'silent':1, 'objective':'binary:logistic'}
        
    # specify validations set to watch performance
    watchlist = [(dtest, 'eval'), (dtrain, 'train')]
    num_round = 2
    model = xgb.train(param, dtrain, num_round, watchlist)

    # Note that this function accesses the trained XGBoost model via an explicit
    # argument, but other state can be captured via closure capture if necessary.
    def predict(model, inputs):
        return [str(model.predict(x)) for x in inputs]
        
        deploy_xgboost_model(
        clipper_conn,
        name="example",
        input_type="doubles",
        func=predict,
        xgboost_model=model)
    """
    
    model_class = type(xgboost_model)
                            if model_class is not "Booster":
                                raise ClipperException(
                                                       "xgboost_model argument was not a xgboost object")

    # save predict function
    serialization_dir = save_python_function(name, func)
    # save Spark model
    xgboost_model_save_loc = os.path.join(serialization_dir,
                                    "xgboost_model_data")
    try:
        xgboost_model.save_model(xgboost_model_save_loc)
    except Exception as e:
        logger.warn("Error saving xgboost model: %s" % e)
        raise e

    
    logger.info("XGBoost model saved")
    
    # Deploy model
    clipper_conn.build_and_deploy_model(name, version, input_type,
                                        serialization_dir, base_image, labels,
                                        registry, num_replicas, batch_size)
                                        
                                        # Remove temp files
    shutil.rmtree(serialization_dir)
