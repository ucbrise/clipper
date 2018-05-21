Container Managers
==================

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