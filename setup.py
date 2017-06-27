from distutils.core import setup
from setuptools import find_packages

setup(
    name='clipper_admin',
    version='0.1.4',
    description='Admin commands for the Clipper prediction-serving system',
    author='Dan Crankshaw',
    author_email='crankshaw@cs.berkeley.edu',
    url='http://clipper.ai',
    packages=['clipper_admin'],
    keywords=['clipper', 'prediction', 'model', 'management'],
    install_requires=[
        'requests', 'pyparsing', 'appdirs', 'pprint', 'subprocess32',
        'sklearn', 'numpy', 'scipy', 'fabric', 'pyyaml'
    ])
