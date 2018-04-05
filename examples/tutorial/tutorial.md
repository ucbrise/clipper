# Tutorial

This tutorial will walk you through the process of starting Clipper,
creating an application, querying the application, and deploying models
to improve application accuracy.

The tutorial uses the Clipper client management Python library for controlling
Clipper. While this is the simplest way to manage Clipper, the client library
is simply a convenience wrapper around a standardized management REST interface
which can be queried with any REST client.


## Setup

The tutorial runs Clipper in Docker containers and orchestrates
them with Docker-Compose, so you must have Docker and Docker-Compose
installed. The tutorial can be run in two modes, local or remote.

Running in local mode will start Clipper locally on your laptop. Running
in remote mode will start Clipper on the machine you specify. When
running the tutorial remotely, you must have SSH access to the machine.
If running on EC2, you can use AMI `ami-3ba0f05b`,
an Ubuntu image that has Docker and Docker-Compose installed.

The tutorial uses Python and Jupyter notebooks to interact with Clipper and
train models. You can install the Python dependencies needed to run
the tutorial with:

```
pip install -r requirements.txt
```

Finally, the tutorial uses the `redis-cli` tool to query Redis directly for some Clipper inspection commands. This tool is usually part of the Redis package.

On a Mac:

```console
brew install redis
```

On Ubuntu 16.04:

```console
apt-get install redis-tools
```

Note the Python and Redis-CLI dependencies must be installed for both the local and remote versions of the tutorial.

## Running the tutorial

To start the tutorial, start the Jupyter server in this directory (`examples/tutorial`)
```
$ jupyter notebook
```
and open the `tutorial_part_one.ipynb` notebook. The tutorial is self-guided
from there.

Happy serving!

_If you run into any problems or find anything unclear while doing the tutorial,
please file an issue or contact us (<clipper-dev@googlegroups.com>) so we
can improve it._

