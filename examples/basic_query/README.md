
# Basic Query Example Requirements

The examples in this directory assume you have the `clipper_admin` pip package installed:

```sh
pip install clipper_admin
```
We recommend using [Anaconda](https://www.continuum.io/downloads)
to install Python packages. 

# Running the example query

Run the example: `python example_client.py`
Note that in the version 0.3, Python3.7 is not supported.

## Possible Errors
Sometimes, because of version issues, you can see this error logs from docker log. 
```
$ docker logs 439ba722d79a
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

In this case, you can fix this issue by installing cloudpickle dependency. 
`pip install -U cloudpickle==0.5.3`

## Model Function
```python
def feature_sum(xs):                                                                                                                                                                                    
    return [str(sum(x)) for x in xs]
 ```  

Define a simple model that just returns the sum of each feature vector. Note that the prediction function takes a list of feature vectors as input and returns a list of strings.

## Basic Example Code Explanation
Let's import necessary classes for applications and models registration. Our model in the example will be a Python function.
```python
from clipper_admin import ClipperConnection, DockerContainerManager                                                                                                                                     
from clipper_admin.deployers import python as python_deployer     
```

We first create a connection to a clipper cluster. Note that we should define a container manager when we initiate a connection. 
In this example, we will use a Docker container manager. 
```python
clipper_conn = ClipperConnection(DockerContainerManager()) 

"""
DockerContainerManager uses Docker-Compose to orchestrate the Clipper cluster. 
If you want to use Kubernetes for orchestration, use KubernetesContainerManager instead. 
You can also create your own ContainerManager by inheriting container_manager class.
"""
```

Now, let's create a Clipper cluster. start_clipper() function creates containers for a Clipper cluster. 
It includes Query Frontend, Prometheus metric server, Frontend management, and Redis. 
 
```python
clipper_conn.start_clipper() 
```

Once we start a Clipper cluster, we can see the log that our cluster is running.

```
18-11-23:16:56:41 INFO     [docker_container_manager.py:151] [default-cluster] Starting managed Redis instance in Docker
18-11-23:16:59:28 INFO     [docker_container_manager.py:229] [default-cluster] Metric Configuration Saved at /private/var/folders/b7/gfcqcp6n1qv63rkfpkn_0qnjcxxskx/T/tmp2l4pspy4.yml
18-11-23:16:59:28 INFO     [clipper_admin.py:138] [default-cluster] Clipper is running
```

Once Clipper cluster is started, we should register applications and models. In our case, our model will be a python function, `feature_sum`, we created above.
We will use a python deployer to deploy our application and model.
Check out the link to learn what is model deployers and what model deployers Clipper currently has. http://docs.clipper.ai/en/v0.3.0/model_deployers.html  
```python
python_deployer.create_endpoint(clipper_conn, "simple-example", "doubles", 46, feature_sum) 
```

python_deployer (clipper_admin.deployers.python) registers application, deploys a model, and link the model to the app. (It can be changed in the later update. It is written when the verion is 0.3) 
If you want to create your own Dockerfile, don't forget that you should register the application and connect the model to the application.
Also, from the following link, see the explanation about `Applications` to understand the concept of Clipper applications and why you need to connect your model to application.
http://clipper.ai/tutorials/basic_concepts/

```python
clipper_conn.register_application(name, input_type, default_output,
                              slo_micros)
deploy_python_closure(clipper_conn, name, version, input_type, func,
                  base_image, labels, registry, num_replicas,
                  batch_size, pkgs_to_install)

clipper_conn.link_model_to_app(name, name)
```

We can see these logs when the registration is succesfully done.

```
18-11-23:16:59:28 INFO     [clipper_admin.py:215] [default-cluster] Application simple-example was successfully registered
18-11-23:16:59:28 INFO     [deployer_utils.py:41] Saving function to /var/folders/b7/gfcqcp6n1qv63rkfpkn_0qnjcxxskx/T/tmp_810jx8yclipper
18-11-23:16:59:28 INFO     [deployer_utils.py:51] Serialized and supplied predict function
18-11-23:16:59:28 INFO     [python.py:192] Python closure saved
18-11-23:16:59:28 INFO     [python.py:206] Using Python 3.6 base image
18-11-23:16:59:28 INFO     [clipper_admin.py:467] [default-cluster] Building model Docker image with model data from /var/folders/b7/gfcqcp6n1qv63rkfpkn_0qnjcxxskx/T/tmp_810jx8yclipper
18-11-23:16:59:43 INFO     [clipper_admin.py:472] [default-cluster] Step 1/2 : FROM clipper/python36-closure-container:develop
18-11-23:16:59:43 INFO     [clipper_admin.py:472] [default-cluster]  ---> 0fac6e6e8242
18-11-23:16:59:43 INFO     [clipper_admin.py:472] [default-cluster] Step 2/2 : COPY /var/folders/b7/gfcqcp6n1qv63rkfpkn_0qnjcxxskx/T/tmp_810jx8yclipper /model/
18-11-23:16:59:43 INFO     [clipper_admin.py:472] [default-cluster]  ---> 4a2709ceaa4f
18-11-23:16:59:43 INFO     [clipper_admin.py:472] [default-cluster] Successfully built 4a2709ceaa4f
18-11-23:16:59:43 INFO     [clipper_admin.py:472] [default-cluster] Successfully tagged default-cluster-simple-example:1
18-11-23:16:59:43 INFO     [clipper_admin.py:474] [default-cluster] Pushing model Docker image to default-cluster-simple-example:1
18-11-23:16:59:46 INFO     [docker_container_manager.py:353] [default-cluster] Found 0 replicas for simple-example:1. Adding 1
18-11-23:16:59:53 INFO     [clipper_admin.py:651] [default-cluster] Successfully registered model simple-example:1
18-11-23:16:59:53 INFO     [clipper_admin.py:569] [default-cluster] Done deploying model simple-example:1.
18-11-23:16:59:53 INFO     [clipper_admin.py:277] [default-cluster] Model simple-example is now linked to application simple-example
```

Now that you’ve deployed your first model, you can start requesting predictions with your favorite REST client at the endpoint that Clipper created for your application: 
`http://localhost:1337/<your-app>/predict`
To check this out, try out this command. 
```
curl -X POST --header "Content-Type:application/json" -d '{"input": [1.1, 2.2, 3.3]}' 127.0.0.1:1337/simple-example/predict
```
