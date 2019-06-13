
# Basic Query Example Requirements

The examples in this directory assume you have the `clipper_admin` pip package installed:

```sh
pip install clipper_admin
```
We recommend using [Anaconda](https://www.continuum.io/downloads) or virtualenv
to install Python packages.  

# Running the example query

Run the example: `python example_client.py`
```
This quickstart requires Docker and supports Python 2.7, 3.5, 3.6 and 3.7.
```
Note that in for the Clipper version 0.3, Python3.7 is not supported.

## Possible Errors
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

Please report an issue if you have another problem when running basic_query example.

# Code explanation

### Model Function
```python
def feature_sum(xs):                                                                                                                                                                                    
    return [str(sum(x)) for x in xs]
 ```  

Define a simple model that just returns the sum of each feature vector. In this case, our model is a python function. Clipper can deploy a python function as a model. Note that the prediction function takes a list of feature vectors as input and returns a list of strings. <br/> <br />
Clipper's model predict functions have a rule to follow in terms of input & output format & types. Check out the link for further details
http://clipper.ai/tutorials/basic_concepts/#model-deployment 

### API
Let's import necessary classes. Details about each API will be here. http://docs.clipper.ai/en/v0.3.0/
```python
from clipper_admin import ClipperConnection, DockerContainerManager                                                                                                                                     
from clipper_admin.deployers import python as python_deployer     
```

### Clipper Connection
We first create a connection class. ClipperConnection is the primary way of starting and managing a Clipper cluster. 
Note that we should define a container manager when we initiate a connection. 
In this example, we will use a Docker container manager. 
```python
clipper_conn = ClipperConnection(DockerContainerManager()) 

"""
DockerContainerManager uses Docker to orchestrate the Clipper cluster. 
If you want to use Kubernetes for orchestration, use KubernetesContainerManager instead. 
You can also create your own ContainerManager by inheriting container_manager class.
"""
```

### Clipper Cluster
Now, let's create a Clipper cluster. start_clipper() function creates and runs necessary containers for the Clipper cluster and connects to it.
 
It includes Query Frontend, Prometheus metric server, Frontend management, Frontend Exporter, and Redis. <br /><br />
You can learn about the architecture here as well. http://clipper.ai/tutorials/basic_concepts/#a-clipper-cluster
 
```python
clipper_conn.start_clipper()
# Note that if the cluster is already created by start_clipper(), you can just connect to it using clipper_conn.connect() function. 
```

Once we start a Clipper cluster, we can see the log that our containers are created & running. You can see the log `Clipper is running` once the cluster is all set.

```
18-11-23:16:56:41 INFO     [docker_container_manager.py:151] [default-cluster] Starting managed Redis instance in Docker
18-11-23:16:59:28 INFO     [docker_container_manager.py:229] [default-cluster] Metric Configuration Saved at /private/var/folders/b7/gfcqcp6n1qv63rkfpkn_0qnjcxxskx/T/tmp2l4pspy4.yml
18-11-23:16:59:28 INFO     [clipper_admin.py:138] [default-cluster] Clipper is running
```

### Deploy models
Now, our cluster is ready to go. We should register applications and models. In our case, our model will be a python function, `feature_sum`, we created above.

We will use a python deployer to deploy our application and model. model deployer will bind a model to our rpc container and deploy it to the cluster.
Check out the link to learn what is model deployers and what model deployers Clipper currently has. http://docs.clipper.ai/en/v0.3.0/model_deployers.html  
```python
python_deployer.create_endpoint(clipper_conn, "simple-example", "doubles", 46, feature_sum) 
```

### Applications & Models
python_deployer (clipper_admin.deployers.python) registers application, deploys a model, and link the model to the app. 
If you want to create your own model Dockerfile, don't forget that you should register the application and connect the model to the application.
Also, from the following link, see the explanation about `Applications` to understand the concept of Clipper applications and why you need to connect your model to application.
http://clipper.ai/tutorials/basic_concepts/

```python
# python_deployer.create_endpoint(clipper_conn, "simple-example", "doubles", 46, feature_sum) is equivalent to 
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

### Query model containers
Now that you’ve deployed your first model, you can start requesting predictions with your favorite REST client at the endpoint that Clipper created for your application: 
`http://localhost:1337/<your-app>/predict`
To check this out, try out this command. 
```
curl -X POST --header "Content-Type:application/json" -d '{"input": [1.1, 2.2, 3.3]}' 127.0.0.1:1337/simple-example/predict
```

Also, for this example, the code will keep querying the query frontend. You can see the latency of feature_sum model from the `predict` function defined by this tutorial.
```python
def predict(addr, x, batch=False):
    url = "http://%s/simple-example/predict" % addr

    if batch:
        req_json = json.dumps({'input_batch': x})
    else:
        req_json = json.dumps({'input': list(x)})

    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))
``` 

### Clean up
You should stop Clipper cluster if you don't use it anymore. 
```python
clipper_conn.stop_all() # stop all the containers in the Clipper cluster and destroy them
```

Note that the Docker Clipper Network is not destoryed by `clipper_conn.stop_all()` function.