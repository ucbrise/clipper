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
        "clipper_admin", "clipper_admin.docker", "clipper_admin.k8s",
        "clipper_admin.deployers", "clipper_admin.deployers.contrib"
    ],
    package_data={'clipper_admin': ['*.txt']},
    keywords=['clipper', 'prediction', 'model', 'management'],
    install_requires=[
        'requests',
        'pyparsing',
        'appdirs',
        'pprint',
        'subprocess32',
        'sklearn',
        'numpy',
        'scipy',
        'pyyaml',
        'docker',
        'kubernetes',
        'six',
    ],
    extras_require={'TensorFlow': ['tensorflow'],
                    'RPython': ['rpy2']})
