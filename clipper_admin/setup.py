from setuptools import setup
import os

with open('README.rst') as f:
    readme = f.read()

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
    license='Apache-2.0',
    packages=[
        "clipper_admin", "clipper_admin.docker", "clipper_admin.kubernetes",
        "clipper_admin.deployers", "clipper_admin.metrics", "clipper_admin.docker.logging"
    ],
    package_data={'clipper_admin': ['*.txt', '*/*.yaml']},
    keywords=['clipper', 'prediction', 'model', 'management'],
    install_requires=[
        'requests>=2.20.0', 'numpy', 'subprocess32; python_version<"3"', 'pyyaml>=4.2b1',
        'docker>=3.0', 'kubernetes>=6.0.0', 'prometheus_client',
        'cloudpickle==0.5.3', 'enum34; python_version<"3.4"', 'redis', 'psutil',
        'jsonschema', 'jinja2'
    ],
    extras_require={
        'PySpark': ['pyspark'],
        'TensorFlow': ['tensorflow'],
    })
