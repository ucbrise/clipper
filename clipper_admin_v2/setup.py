from setuptools import setup

with open('README.rst') as f:
    readme = f.read()

with open('LICENSE.txt') as f:
    license = f.read()

version = {}
with open("clipper_admin/version.py") as fp:
    exec(fp.read(), version)

setup(
    name='clipper_admin',
    version=version['__version__'],
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
