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


**Note: This quickstart requires [Docker](https://www.docker.com/) and [Docker Compose](https://docs.docker.com/compose/).**


#### Start a Clipper Instance and Deploy a Model

```
$ pip install clipper_admin
$ python
>>> from clipper_admin import Clipper
>>> import numpy as np
# Start a Clipper instance on localhost
>>> clipper = Clipper("localhost")
Checking if Docker is running...

# Start Clipper. Running this command for the first time will
# download several Docker containers, so it may take some time.
>>> clipper.start()
Clipper is running

# Register an application called "hello_world" that will query a model
# called "feature_sum_model". This will create a prediction REST endpoint at
# http://localhost:1337/hello_world/predict
>>> clipper.register_application("hello_world", "feature_sum_model", "doubles", "-1.0", 100000)
Success!

# Inspect Clipper to see the registered apps
>>> clipper.get_all_apps()
[u'hello_world']

# Define a simple model that just returns the sum of each feature vector.
# Note that the prediction function takes a list of feature vectors as
# input and returns a list of strings.
>>> def feature_sum_function(xs):
      return [str(np.sum(x)) for x in xs]

# Deploy the model, naming it "feature_sum_model" and giving it version 1
>>> clipper.deploy_predict_function("feature_sum_model", 1, feature_sum_function, "doubles")

```

#### Query Clipper for predictions


Now that you've deployed your first model, you can start requesting predictions at the
REST endpoint that clipper created for your application:
`http://localhost:1337/hello_world/predict`

With curl:


```console
$ curl -X POST --header "Content-Type:application/json" -d '{"uid": 0, "input": [1.1, 2.2, 3.3]}' 127.0.0.1:1337/hello_world/predict
```

From a Python REPL:

```py
>>> import requests, json, numpy as np
>>> headers = {"Content-type": "application/json"}
>>> requests.post("http://localhost:1337/hello_world/predict", headers=headers, data=json.dumps({"uid": 0, "input": list(np.random.random(10))})).json()
```


## Contributing

To file a bug or request a feature, please file a GitHub issue. Pull requests are welcome. Additional help and instructions
for contributors can be found on our website at <http://clipper.ai/contributing>.

## The Team

+ Dan Crankshaw (@dcrankshaw)
+ Joey Gonzalez (@jegonzal)
+ Corey Zumar (@Corey-Zumar)
+ Nishad Singh (@nishadsingh1)
+ Alexey Tumanov (@atumanov)

You can contact us at <clipper-dev@googlegroups.com>

## Acknowledgements

This research is supported in part by DHS Award HSHQDC-16-3-00083, DOE Award SN10040 DE-SC0012463, NSF CISE Expeditions Award CCF-1139158, and gifts from Ant Financial, Amazon Web Services, CapitalOne, Ericsson, GE, Google, Huawei, Intel, IBM, Microsoft and VMware.
