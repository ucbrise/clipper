

Key Concepts
============

A model deployment in Clipper has three elements:

1. *An application*
2. *A model*
3. *Model containers*

API Documentation
==================

Clipper Connection
-------------------

The main object used to 

All of Requests' functionality can be accessed by these 7 methods.

.. autoclass:: clipper_admin.ClipperConnection
    :members:
    :undoc-members:
    :show-inheritance:

Exceptions
----------

.. autoexception:: clipper_admin.ClipperException
.. autoexception:: clipper_admin.UnconnectedException


Container Managers
------------------

.. autoclass:: clipper_admin.container_manager.ContainerManager
    :members:
    :undoc-members:
    :show-inheritance:

.. autoclass:: clipper_admin.DockerContainerManager
    :members:
    :undoc-members:
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

