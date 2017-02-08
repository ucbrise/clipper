# Demo

This demo walks the user through setting up Clipper, deploying models,
and querying Clipper for predictions.

## Setup

In order to run the demo, you must either have Docker installed locally
or have access to a machine running Docker that you can SSH into.
If running on EC2, you can use AMI `ami-3ba0f05b`,
an Ubuntu image that has Docker and Docker-Compose installed.

The demo uses Python and Jupyter notebooks to interact with Clipper and
train models. You can install the Python dependencies with

```
$ pip install -r requirements.txt
```

## Running the demo

To start the demo, start the Jupyter server in this directory (`examples/cifar_demo`).

```
$ jupyter notebook
```

The demo starts with `rise_demo_frontend_dev.ipynb` and is self-guided
within the notebooks from there.
