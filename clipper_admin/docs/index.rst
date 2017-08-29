API Documentation
==================

Clipper Connection
-------------------

``ClipperConnection`` is the primary way of starting and managing a Clipper cluster.

.. autoclass:: clipper_admin.ClipperConnection
    :members:
    :undoc-members:


Container Managers
------------------

Container managers abstract away the low-level tasks of creating and managing Clipper's
Docker containers in order to allow Clipper to be deployed with different container
orchestration frameworks. Clipper currently provides two container manager implementations,
one for running Clipper locally directly on Docker, and one for running Clipper on Kubernetes.

.. autoclass:: clipper_admin.DockerContainerManager
    :members:
    :show-inheritance:

.. autoclass:: clipper_admin.KubernetesContainerManager
    :members:
    :show-inheritance:

Model Deployers
----------------

TODO: Update the model deployers section

Pure Python functions
~~~~~~~~~~~~~~~~~~~~~
.. autofunction:: clipper_admin.deployers.python.deploy_python_closure
.. autofunction:: clipper_admin.deployers.python.create_endpoint


PySpark Models
~~~~~~~~~~~~~~~~~~~~~
.. autofunction:: clipper_admin.deployers.pyspark.deploy_pyspark_model
.. autofunction:: clipper_admin.deployers.pyspark.create_endpoint


Exceptions
----------

.. autoexception:: clipper_admin.ClipperException
.. autoexception:: clipper_admin.UnconnectedException

