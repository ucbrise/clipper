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

> This quickstart requires [Docker](https://www.docker.com/) and supports Python 2.7, 3.5, 3.6 and 3.7.


#### Clipper Example Code
* Basic query: https://github.com/ucbrise/clipper/tree/develop/examples/basic_query
* Image query: https://github.com/ucbrise/clipper/tree/develop/examples/image_query

* Examples including metrics: https://github.com/ucbrise/clipper/tree/develop/examples
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

Create a `ClipperConnection` object and start Clipper. Running this command for the first time will
download several Docker containers, so it may take some time.

```py
from clipper_admin import ClipperConnection, DockerContainerManager
clipper_conn = ClipperConnection(DockerContainerManager())
clipper_conn.start_clipper()
```

```pycon
17-08-30:15:48:41 INFO     [docker_container_manager.py:95] Starting managed Redis instance in Docker
17-08-30:15:48:43 INFO     [clipper_admin.py:105] Clipper still initializing.
17-08-30:15:48:44 INFO     [clipper_admin.py:107] Clipper is running
```

Register an application called `"hello-world"`. This will create a prediction REST endpoint at `http://localhost:1337/hello-world/predict`

```py
clipper_conn.register_application(name="hello-world", input_type="doubles", default_output="-1.0", slo_micros=100000)
```

```pycon
17-08-30:15:51:42 INFO     [clipper_admin.py:182] Application hello-world was successfully registered
```

Inspect Clipper to see the registered apps

```py
clipper_conn.get_all_apps()
```

```pycon
[u'hello-world']
```

Define a simple model that just returns the sum of each feature vector.
Note that the prediction function takes a list of feature vectors as
input and returns a list of strings.

```py
def feature_sum(xs):
    return [str(sum(x)) for x in xs]
```

Import the python deployer package

```py
from clipper_admin.deployers import python as python_deployer
```

Deploy the `"feature_sum"` function as a model. Notice that the application and model
must have the same input type.

```py
python_deployer.deploy_python_closure(clipper_conn, name="sum-model", version=1, input_type="doubles", func=feature_sum)
```

```pycon
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
```

__Possible Error__
If start_clipper() is stuck at this logs, try `pip install -U cloudpickle==0.5.3`
```
18-05-21:12:19:59 INFO     [deployer_utils.py:44] Saving function to /tmp/clipper/tmpx6d_zqeq
18-05-21:12:19:59 INFO     [deployer_utils.py:54] Serialized and supplied predict function
18-05-21:12:19:59 INFO     [python.py:192] Python closure saved
18-05-21:12:19:59 INFO     [python.py:206] Using Python 3.6 base image
18-05-21:12:19:59 INFO     [clipper_admin.py:451] Building model Docker image with model data from /tmp/clipper/tmpx6d_zqeq
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': 'Step 1/2 : FROM clipper/python36-closure-container:develop'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': '\n'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': ' ---> 1aaddfa3945e\n'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': 'Step 2/2 : COPY /tmp/clipper/tmpx6d_zqeq /model/'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': '\n'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': ' ---> b7c29f531d2e\n'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'aux': {'ID': 'sha256:b7c29f531d2eaf59dd39579dbe512538be398dcb5fdd182db14e4d58770d2055'}}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': 'Successfully built b7c29f531d2e\n'}
18-05-21:12:20:00 INFO     [clipper_admin.py:455] {'stream': 'Successfully tagged sum-model:1\n'}
18-05-21:12:20:00 INFO     [clipper_admin.py:457] Pushing model Docker image to sum-model:1
18-05-21:12:20:02 INFO     [docker_container_manager.py:247] Found 0 replicas for sum-model:1. Adding 1
```

It is because of cloudpickle dependency version issue. You may see this error logs from model container docker log. 
```
$ docker logs 439ba722d79a # model container logs. For this example, it will be simple-example model container
Starting Python Closure container
Connecting to Clipper with default port: 7000
Traceback (most recent call last):
  File "/container/python_closure_container.py", line 56, in <module>
    rpc_service.get_input_type())
  File "/container/python_closure_container.py", line 28, in __init__
    self.predict_func = load_predict_func(predict_path)
  File "/container/python_closure_container.py", line 17, in load_predict_func
    return cloudpickle.load(serialized_func_file)
  File "/usr/local/lib/python3.6/site-packages/cloudpickle/cloudpickle.py", line 1060, in _make_skel_func
    base_globals['__builtins__'] = __builtins__
TypeError: 'str' object does not support item assignment
```

Tell Clipper to route requests for the "hello-world" application to the "sum-model"

```py
clipper_conn.link_model_to_app(app_name="hello-world", model_name="sum-model")
```

```pycon
17-08-30:16:08:50 INFO     [clipper_admin.py:224] Model sum-model is now linked to application hello-world
```

Your application is now ready to serve predictions

#### Query Clipper for predictions


Now that you've deployed your first model, you can start requesting predictions at the REST endpoint that clipper created for your application: `http://localhost:1337/hello-world/predict`

With cURL:


```sh
$ curl -X POST --header "Content-Type:application/json" -d '{"input": [1.1, 2.2, 3.3]}' 127.0.0.1:1337/hello-world/predict
```

With Python:

```py
import requests, json, numpy as np
headers = {"Content-type": "application/json"}
requests.post("http://localhost:1337/hello-world/predict", headers=headers, data=json.dumps({"input": list(np.random.random(10))})).json()
```

#### Clean up

If you closed the Python REPL you were using to start Clipper, you will need to start a new Python REPL and create another connection to the Clipper cluster. If you still have the Python REPL session active from earlier, you can re-use your existing `ClipperConnection` object.


Create a new connection. If you have still have the Python REPL from earlier, you can skip this step.

```py
from clipper_admin import ClipperConnection, DockerContainerManager
clipper_conn = ClipperConnection(DockerContainerManager())
clipper_conn.connect()
```
Stop all Clipper docker containers

```py
clipper_conn.stop_all()
```

```pycon
17-08-30:16:15:38 INFO     [clipper_admin.py:1141] Stopped all Clipper cluster and all model containers
```


## Contributing

To file a bug or request a feature, please file a GitHub issue. Pull requests are welcome. Additional help and instructions for contributors can be found on our website at <http://clipper.ai/contributing>.

## The Team

+ [Dan Crankshaw](https://github.com/dcrankshaw)
+ [Corey Zumar](https://github.com/Corey-Zumar)
+ [Joey Gonzalez](https://github.com/jegonzal)
+ [Alexey Tumanov](https://github.com/atumanov)
+ [Eyal Sela](https://github.com/EyalSel)
+ [Simon Mo](https://github.com/simon-mo)
+ [Rehan Durrani](https://github.com/RehanSD)
+ [Eric Sheng](https://github/com/es1024)

You can contact us at <clipper-dev@googlegroups.com>

## Acknowledgements

This research is supported in part by DHS Award HSHQDC-16-3-00083, DOE Award SN10040 DE-SC0012463, NSF CISE Expeditions Award CCF-1139158, and gifts from Ant Financial, Amazon Web Services, CapitalOne, Ericsson, GE, Google, Huawei, Intel, IBM, Microsoft and VMware.
