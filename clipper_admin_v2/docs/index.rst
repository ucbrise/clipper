API Documentation
==================

Clipper Connection
-------------------

The main object used to 

.. autoclass:: clipper_admin.ClipperConnection
    :members:
    :undoc-members:

Exceptions
----------

.. autoexception:: clipper_admin.ClipperException
.. autoexception:: clipper_admin.UnconnectedException


Container Managers
------------------

.. autoclass:: clipper_admin.DockerContainerManager
    :members:
    :show-inheritance:

.. autoclass:: clipper_admin.K8sContainerManager
    :members:
    :show-inheritance:

Model Deployers
----------------

Pure Python functions
~~~~~~~~~~~~~~~~~~~~~
.. autofunction:: clipper_admin.deployers.python.deploy_python_closure
.. autofunction:: clipper_admin.deployers.python.create_endpoint


PySpark Models
~~~~~~~~~~~~~~~~~~~~~
.. autofunction:: clipper_admin.deployers.pyspark.deploy_pyspark_model
.. autofunction:: clipper_admin.deployers.pyspark.create_endpoint

