Model Deployers
===============

Clipper provides a collection of model deployer modules to simplify the process of deploying
a trained model to Clipper and avoid the need to figure out how to save models and
build custom Docker containers capable of serving the saved models for some common use
cases. With these modules, you can deploy models directly from Python to Clipper.

Currently, Clipper provides the following deployer modules:

1. Arbitrary Python functions
2. PySpark Models
3. PyTorch Models
4. Tensorflow Models
5. MXNet Models
6. PyTorch Models exported as ONNX file with Caffe2 Serving Backend (Experimental)
7. Keras Models

These deployers support function that can only be pickled using 
`Cloudpickle <https://github.com/cloudpipe/cloudpickle>`_ and/or 
pure python libraries that can be installed  via `pip`. For reference, 
please use the following flowchart to make decision about which deployer 
to use. 

.. graphviz::

   digraph foo {
      "Pure Python?" -> "Use python deployer & pkgs_to_install arg" [ label="Yes" ];
      "Pure Python?" -> "Does Clipper provide a deployer?" [ label="No" ];
      "Does Clipper provide a deployer?" -> "Use {PyTorch | TensorFlow | PySpark | ...} deployers" [ label="Yes" ];
      "Does Clipper provide a deployer?" -> "Build your own container" [ label="No" ];
   }

.. note::
    You can find additional examples of using model deployers in
    `Clipper's integration tests <https://github.com/ucbrise/clipper/tree/develop/integration-tests>`_.

Pure Python functions
---------------------

This module supports deploying pure Python function closures to Clipper. A function deployed with
this module must take a list of inputs as the sole argument, and return a list of strings of
exactly the same length. The reason the prediction function takes a list of inputs rather than
a single input is to provide models the possibility of computing multiple predictions in parallel
to improve model performance. For example, many models that run on a GPU can significantly improve
throughput by batching predictions to better utilize the many parallel cores of the GPU.

In addition, the function must only use pure Python code. More specifically, all of the state
captured by the function will be pickled using `Cloudpickle <https://github.com/cloudpipe/cloudpickle>`_,
so any state captured by the function must be able to be pickled. Most Python libraries that
use C extensions create objects that cannot be pickled. This includes many common machine-learning
frameworks such as PySpark, TensorFlow, PyTorch, and Caffe. You will have to use Clipper provided
containers or create your own Docker containers and call the native serialization libraries of these
frameworks in order to deploy them. 

While this deployer will serialize your function, any Python libraries that the function depends
on must be installed in the container to be able to load the function inside the model container.
You can specify these libraries using the ``pkgs_to_install`` argument. All the packages specified by
that argument will be installed in the container with pip prior to running it.

If your function has dependencies that cannot be installed directly with pip, you will need to build your
own container.


.. autofunction:: clipper_admin.deployers.python.deploy_python_closure
.. autofunction:: clipper_admin.deployers.python.create_endpoint


PySpark Models
--------------

The PySpark model deployer module provides a small extension to the Python closure deployer to allow
you to deploy Python functions that include PySpark models as part of the state. PySpark models
cannot be pickled and so they break the Python closure deployer. Instead, they must be saved using
the native PySpark save and load APIs. To get around this limitation, the PySpark model deployer
introduces two changes to the Python closure deployer discussed above.

First, a function deployed with this module *takes two additional arguments*: a PySpark ``SparkSession`` object
and a PySpark model object, along with a list of inputs as provided to the Python closures in the
``deployers.python`` module. It must still return a list of strings of the same length as the list
of inputs.

Second, the ``pyspark.deploy_pyspark_model`` and ``pyspark.create_endpoint`` deployment methods
introduce two additional arguments:

* ``pyspark_model``: A PySpark model object. This model will be serialized using the native PySpark
  serialization API and loaded into the deployed model container. The model container creates a
  long-lived SparkSession when it is first initialized and uses that to load this model once at initialization
  time. The long-lived SparkSession and loaded model are provided by the container as arguments to the prediction
  function each time the model container receives a new prediction request.
* ``sc``: The current SparkContext. The PySpark model serialization API requires the SparkContext as an argument

The effect of these two changes is to allow the deployed prediction function to capture all pure Python
state through closure capture but explicitly declare the additional PySpark state which must be saved and
loaded through a separate process.

.. autofunction:: clipper_admin.deployers.pyspark.deploy_pyspark_model
.. autofunction:: clipper_admin.deployers.pyspark.create_endpoint


PyTorch Models
--------------
Similar to the PySpark deployer, the PyTorch deployer provides a small extension to the Python closure deployer
to allow you to deploy Python functions that include PyTorch models.

For PyTorch, Clipper will serialize the model using ``torch.save`` and it will be loaded using ``torch.load``. It is expected the model has a forward method and can be called using ``model(input)`` to predict output.

.. autofunction:: clipper_admin.deployers.pytorch.deploy_pytorch_model
.. autofunction:: clipper_admin.deployers.pytorch.create_endpoint


Tensorflow Models
-----------------
Similar to the PySpark deployer, the TensorFlow deployer provides a small extension to the Python closure deployer
to allow you to deploy Python functions that include TensorFlow models.

For Tensorflow, Clipper will save the Tensorflow Session.

.. autofunction:: clipper_admin.deployers.tensorflow.deploy_tensorflow_model
.. autofunction:: clipper_admin.deployers.tensorflow.create_endpoint


MXNet Models
------------
Similar to PySpark deployer, the MXNet deployer provides a small extension to the Python closure deployer
to allow you to deploy Python functions that include MXNet models.

For MXNet, Clipper will serialize the model using ``mxnet_model.save_checkpoint(..., epoch=0)``.

.. autofunction:: clipper_admin.deployers.mxnet.deploy_mxnet_model
.. autofunction:: clipper_admin.deployers.mxnet.create_endpoint


Keras Models
------------
Similar to PySpark deployer, the Keras deployer provides a small extension to the Python closure deployer
to allow you to deploy Python functions that include Keras models.

For Keras, Clipper will serialize the model using ``keras_model.save(keras_model.h5)``.

.. autofunction:: clipper_admin.deployers.keras.deploy_keras_model
.. autofunction:: clipper_admin.deployers.keras.create_endpoint


Create Your Own Container
-------------------------

If none of the provided model deployers will meet your needs, you will need to create your own model container.

`Tutorial on building your own model container <http://clipper.ai/tutorials/custom_model_container>`_
