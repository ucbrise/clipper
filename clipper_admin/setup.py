from setuptools import setup
import os

with open('README.rst') as f:
    readme = f.read()

with open('LICENSE.txt') as f:
    license = f.read()

with open(os.path.abspath("clipper_admin/VERSION.txt"), "r") as fp:
    version = fp.read().strip()

setup(
    name='clipper_admin',
    version=version,
    description='Admin commands for the Clipper prediction-serving system',
    long_description=readme,
    maintainer='Dan Crankshaw',
    maintainer_email='crankshaw@cs.berkeley.edu',
    url='http://clipper.ai',
    license=license,
    packages=[
        "clipper_admin", "clipper_admin.docker", "clipper_admin.kubernetes",
        "clipper_admin.deployers"
    ],
    package_data={'clipper_admin': ['*.txt', '*/*.yaml']},
    keywords=['clipper', 'prediction', 'model', 'management'],
    install_requires=[
        'requests', 'subprocess32', 'pyyaml', 'docker', 'kubernetes',
        'prometheus_client', 'six', 'cloudpickle>=0.5.2'
    ],
    extras_require={
        'PySpark': ['pyspark'],
        'TensorFlow': ['tensorflow'],
    })
