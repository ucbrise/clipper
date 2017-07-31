from distutils.core import setup
from setuptools import find_packages

version = {}
with open("clipper_admin/version.py") as fp:
    exec(fp.read(), version)

setup(
    name='clipper_admin',
    version=version["version"],
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
