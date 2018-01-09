# Clipper

[![Build Status](https://amplab.cs.berkeley.edu/jenkins/buildStatus/icon?job=Clipper)](https://amplab.cs.berkeley.edu/jenkins/job/Clipper/) [![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)


<img src="images/clipper-logo.png" width="200">


## What is Clipper?

Clipper is a prediction serving system that sits between user-facing applications and a wide range of commonly used machine learning models and frameworks. Learn more about Clipper and view documentation at our website <http://clipper.ai>.

## What does Clipper do?

* Clipper **simplifies integration of machine learning techniques** into user facing applications by providing a simple standard REST interface for prediction and feedback across a wide range of commonly used machine learning frameworks.  *Clipper makes product teams happy.*


* Clipper **simplifies model deployment** and **helps reduce common bugs** by using the same tools and libraries used in model development to render live predictions.  *Clipper makes data scientists happy.*


* Clipper **improves throughput** and ensures **reliable millisecond latencies** by introducing adaptive batching, caching, and straggler mitigation techniques.  *Clipper makes the infra-team less unhappy.*

* Clipper **improves prediction accuracy** by introducing state-of-the-art bandit and ensemble methods to intelligently select and combine predictions and achieve real-time personalization across machine learning frameworks.  *Clipper makes users happy.*


## Quickstart

**Note: This quickstart works for the latest version of code. For a quickstart that works with the released version of Clipper available on PyPi, go to our [website](http://clipper.ai)**

> This quickstart requires [Docker](https://www.docker.com/) and only supports Python2.


#### Start a Clipper Instance and Deploy a Model

__Install Clipper__

You can either install Clipper directly from GitHub:
```sh
pip install git+https://github.com/ucbrise/clipper.git@develop#subdirectory=clipper_admin
```
or by cloning Clipper and installing directly from the file system:
```sh
pip install -e </path/to/clipper_repo>/clipper_admin
```


__Start a local Clipper cluster__

First start a Python interpreter session.

```sh
$ python

# Or start one with iPython
$ conda install ipython
$ ipython
```

```py
>>> from clipper_admin import ClipperConnection, DockerContainerManager
>>> clipper_conn = ClipperConnection(DockerContainerManager())

# Start Clipper. Running this command for the first time will
# download several Docker containers, so it may take some time.
>>> clipper_conn.start_clipper()
17-08-30:15:48:41 INFO     [docker_container_manager.py:95] Starting managed Redis instance in Docker
17-08-30:15:48:43 INFO     [clipper_admin.py:105] Clipper still initializing.
17-08-30:15:48:44 INFO     [clipper_admin.py:107] Clipper is running

# Register an application called "hello_world". This will create
# a prediction REST endpoint at http://localhost:1337/hello_world/predict
>>> clipper_conn.register_application(name="hello-world", input_type="doubles", default_output="-1.0", slo_micros=100000)
17-08-30:15:51:42 INFO     [clipper_admin.py:182] Application hello-world was successfully registered

# Inspect Clipper to see the registered apps
>>> clipper_conn.get_all_apps()
[u'hello_world']

# Define a simple model that just returns the sum of each feature vector.
# Note that the prediction function takes a list of feature vectors as
# input and returns a list of strings.
>>> def feature_sum(xs):
      return [str(sum(x)) for x in xs]

# Import the python deployer package
>>> from clipper_admin.deployers import python as python_deployer

# Deploy the "feature_sum" function as a model. Notice that the application and model
# must have the same input type.
>>> python_deployer.deploy_python_closure(clipper_conn, name="sum-model", version=1, input_type="doubles", func=feature_sum)
17-08-30:15:59:56 INFO     [deployer_utils.py:50] Anaconda environment found. Verifying packages.
17-08-30:16:00:04 INFO     [deployer_utils.py:150] Fetching package metadata .........
Solving package specifications: .

17-08-30:16:00:04 INFO     [deployer_utils.py:151]
17-08-30:16:00:04 INFO     [deployer_utils.py:59] Supplied environment details
17-08-30:16:00:04 INFO     [deployer_utils.py:71] Supplied local modules
17-08-30:16:00:04 INFO     [deployer_utils.py:77] Serialized and supplied predict function
17-08-30:16:00:04 INFO     [python.py:127] Python closure saved
17-08-30:16:00:04 INFO     [clipper_admin.py:375] Building model Docker image with model data from /tmp/python_func_serializations/sum-model
17-08-30:16:00:05 INFO     [clipper_admin.py:378] Pushing model Docker image to sum-model:1
17-08-30:16:00:07 INFO     [docker_container_manager.py:204] Found 0 replicas for sum-model:1. Adding 1
17-08-30:16:00:07 INFO     [clipper_admin.py:519] Successfully registered model sum-model:1
17-08-30:16:00:07 INFO     [clipper_admin.py:447] Done deploying model sum-model:1.

# Tell Clipper to route requests for the "hello-world" application to the "sum-model"
>>> clipper_conn.link_model_to_app(app_name="hello-world", model_name="sum-model")
17-08-30:16:08:50 INFO     [clipper_admin.py:224] Model sum-model is now linked to application hello-world

# Your application is now ready to serve predictions
```

#### Query Clipper for predictions


Now that you've deployed your first model, you can start requesting predictions at the REST endpoint that clipper created for your application: `http://localhost:1337/hello-world/predict`

With cURL:


```sh
$ curl -X POST --header "Content-Type:application/json" -d '{"input": [1.1, 2.2, 3.3]}' 127.0.0.1:1337/hello-world/predict
```

From a Python REPL:

```py
>>> import requests, json, numpy as np
>>> headers = {"Content-type": "application/json"}
>>> requests.post("http://localhost:1337/hello-world/predict", headers=headers, data=json.dumps({"input": list(np.random.random(10))})).json()
```

#### Clean up

If you closed the Python REPL you were using to start Clipper, you will need to start a new Python REPL and create another connection to the Clipper cluster. If you still have the Python REPL session active from earlier, you can re-use your existing `ClipperConnection` object.

```py
# If you have still have the Python REPL from earlier, skip directly
# to clipper_conn.stop_all()
>>> from clipper_admin import ClipperConnection, DockerContainerManager
>>> clipper_conn = ClipperConnection(DockerContainerManager())
>>> clipper_conn.connect()

# Stop all Clipper docker containers
>>> clipper_conn.stop_all()
17-08-30:16:15:38 INFO     [clipper_admin.py:1141] Stopped all Clipper cluster and all model containers
```


## Contributing

To file a bug or request a feature, please file a GitHub issue. Pull requests are welcome. Additional help and instructions for contributors can be found on our website at <http://clipper.ai/contributing>.

## The Team

GitHub usernames are in parentheses.

+ Dan Crankshaw (`dcrankshaw`)
+ Corey Zumar (`Corey-Zumar`)
+ Joey Gonzalez (`jegonzal`)
+ Nishad Singh (`nishadsingh1`)
+ Alexey Tumanov (`atumanov`)
+ Feynman Liang (`feynmanliang`)

You can contact us at <clipper-dev@googlegroups.com>

## Acknowledgements

This research is supported in part by DHS Award HSHQDC-16-3-00083, DOE Award SN10040 DE-SC0012463, NSF CISE Expeditions Award CCF-1139158, and gifts from Ant Financial, Amazon Web Services, CapitalOne, Ericsson, GE, Google, Huawei, Intel, IBM, Microsoft and VMware.
