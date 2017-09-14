The **containers** module contains language-specific model container base images, as well as language-specific utilities for building and deploying models on top of these images. 

Python-specific model deployers can be found in the [clipper_admin directory](clipper_admin/clipper_admin/deployers). These are in a different location because the admin package is pip-installable and should provide all of the deployers directly. The underlying philosophy is that users should only have to install a single model deployment package for each major framework or language.
