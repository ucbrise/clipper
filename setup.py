from distutils.core import setup
from setuptools import find_packages

setup(
    name='clipper_admin',
    version='0.1',
    description='A low latency online prediction serving system',
    author='Dan Crankshaw',
    author_email='dcrankshaw@berkeley.edu',
    url='https://github.com/ucbrise/clipper',
    packages=['clipper_admin'],
    keywords=['prediction', 'model', 'management'],
    install_requires=[
        'requests', 'pyparsing', 'appdirs', 'pprint', 'subprocess32',
        'sklearn', 'numpy', 'scipy', 'fabric', 'conda', 'pyyaml'
    ])
