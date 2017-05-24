# Implementing your own model-container
Clipper allows for quick deployment of custom models. In order to query these models in a live application, they need to be deployed in model-containers that implement a specific Clipper interface.

The good news is that you only need to define your model-container once per model. The bad news is that it can be an involved process. This document will walk you through the process of creating your model-container and deploying your model within it.

If your model can be encapsulated in a Python function closure, you don't need to go through the trouble of defining your own model-container. Clipper supports a simplified process for deploying arbitrary Python prediction functions as model-containers. You can read about it [here](https://conda.io/docs/using/envs.html).

## Defining your model-container
The heart of your model-container is in its implementation of the model-container API. The rest relates to model initialization and connecting to clipper.

You can refer to [sklearn\_cifar\_container.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py) as a simple, complete model-container implementation.

### The model-container API
Your model will be loaded into a model-container that implements a specific API. When deployed, the model-container will communicate with Clipper's Query Frontend API through the following interface defined in [rpc.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py):

```python
class ModelContainerBase(object):
    def predict_ints(self, inputs):
        pass
    
    def predict_floats(self, inputs):
        pass
    
    def predict_doubles(self, inputs):
        pass
    
    def predict_bytes(self, inputs):
        pass
    
    def predict_strings(self, inputs):
        pass
```

Every Clipper application has an associated `input_type`. Queries to the application will be routed to `predict_<input_type>` for all of the application's associated model-containers. Your model-container will extend `rpc.ModelContainerBase` and implement at least one of the `predict_<x>` functions. Ultimately, your `predict_<x>` implementations will make use of your model to deliver actual predictions.

### Implementing `predict_<input_type>`

All of the `predict_<input_type>` functions expect as input a numpy array or Python list of datapoints. These datapoints are, themselves, numpy arrays or Python lists with entries of type `input_type `. The valid `input_type`s are: `ints`, `floats`, `doubles`, `bytes`, and `strings`. The `predict_<input_type`> functions should return a a list of strings that is the same length as the inputs. These can be JSON-format strings but do not need to be.

For the sake of demonstration, we will assume that the Clipper application(s) using your model-container have an `input_type` of `doubles`. In this case, your model-container would extend `rpc.ModelContainerBase` and implement `predict_doubles`. Here's a valid, though not very useful, implementation taken from [noop_container.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/noop_container.py):

```python
def predict_doubles(self, inputs):
    outputs = []
    for input_item in inputs:
        outputs.append(str(sum(input_item)))
    return outputs
```    

A more useful `predict_doubles` implementation would make use of an actual model to serve predictions. [sklearn\_cifar\_container.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py) does exactly that:

```python
def predict_doubles(self, inputs):
    preds = self.model.predict(inputs)
    return [str(p) for p in preds]
```

This implementation makes use of `self.model`, which is set to an actual model upon your model-container's initialization. Note that, in varying `self.model`, you can vary the behavior of `predict_doubles`. This design allows you to update your prediction serving logic without changing your model-container code; simply update your model and supply it to a new model-container instance.

Details on setting `self.model` are in the _Initializing your model-container_ section below.

### Connecting to Clipper
For your model-container to be functional, it needs to be connected to Clipper. The code responsible for this should be run when the model-container is executed. [Everything under the line](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py#L21-L63) `if __name__ == "__main__":` in [sklearn\_cifar\_container.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py) should serve as an example of how to do this.

A call to the Clipper Management Library's `deploy_model` will spin up a fresh container and set several of its environment variables. Four of them –  `CLIPPER_MODEL_NAME`, `CLIPPER_MODEL_VERSION`, `CLIPPER_IP`, and `CLIPPER_PORT` –  are used to connect to Clipper. In almost all cases, using [this code](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py#L22-L48) should suffice.

### Initializing your model-container
The last two environment variables – `CLIPPER_INPUT_TYPE` and `CLIPPER_MODEL_PATH`– can used to initialize your model-container.

`CLIPPER_MODEL_PATH ` should store the path to your serialized model. All you need to do is supply a new instance of your model-container object the path. Your model-container's `__init__` function can deserialize your model from that path and set the model to an instance variable. [sklearn\_cifar\_container.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py) does this [here](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py#L11-L14).

You can also (optionally) run any checks you want, verifying that the serialized files are as expected. [sklearn\_cifar\_container.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py) does this [here](https://github.com/ucbrise/clipper/blob/develop/containers/python/sklearn_cifar_container.py#L51-L60).

Lastly, you need to start the Clipper RPC service provided by `rpc.py`:

```python
rpc_service = rpc.RPCService()
rpc_service.start(model, ip, port, model_name, model_version, input_type)
```

### Logging
Python `print` statements made in your model-container will show up as logs later and won't interfere with prediction serving. It is highly reccomended that you log in many places throughout your model-container code to make debugging easier and provide greater clarity to your model-container's behavior.

## Setting up your Docker image
This section makes the assumption that you are using Docker containers. If the Docker-specific instructions here ever conflict with [Docker's official documentation](https://docs.docker.com/), please defer to the official documentation. If you do not have Docker downloaded, refer to [the official instructions](https://docs.docker.com/docker-for-mac/install/#install-and-run-docker-for-mac).

In order to make use of your model-container implementation, you need to create a Docker image that runs it. Docker images specify what Docker containers do upon startup. Eventually, you will supply an address to this Docker image when deploying your model.

### Defining your Dockerfile

The first step to creating a Docker image is creating a `Dockerfile`. We will assume that your completed model-container implementation is in a file named `model_container.py`.

A standard `Dockerfile` would use `clipper/py-rpc:latest` as its base image, copy your local `model_container.py`, and run it. Such a file (named `Dockerfile` and located in the same directory as `model_container.py`) would look something like this:

```
FROM clipper/py-rpc:latest

MAINTAINER Your Name <yourname@email.com>

COPY model_container.py /container/

CMD ["python", "/container/model_container.py"]
```

Your use case may necessitate that the Dockerfile takes more complex actions. For instance, your model may have dependencies that need to be installed. This sort of thing is definitely possible. Please refer to [the official documentation](https://docs.docker.com/) for details on how to do so.

### Hosting your Docker image
Once you have your Dockerfile written, you can run `docker build .` from within the directory that contains it. If the command completes successfully, you should have a created a local Docker image.

The most recently created image listed in in `docker images` should correspond to the one you just built. Identify its `id` under the `IMAGES` column. You can now supply this `id` as the `container_name` field in the Clipper Management Libarary's `deploy_model` function.

If you ever want to access this same image from another machine without re-building it, you need to host your image remotely. Go ahead and create an account on [Dockerhub](https://hub.docker.com/). Let's assume that you want to be able to identify your image through another label, `my_container`. On your Dockerhub account, create a new repository with the name `my_container`.

Now, we need to push your local image to your newly created repository, `your_username/my_container`. You can do this by tagging your image locally using `docker tag <id> your_username/my_container:latest`, authenticating with Docker using `docker login`, and pushing to the repository with `docker push your_username/my_container:latest`.

## Deploy your model

Get Clipper set up as usual:

```python
import clipper_admin.clipper_manager as cm
user, key, host = ...
clipper = cm.Clipper(host, user, key)
```

Define and serialize your model:

```python
training_data = several data points with entries of type <input_type>
model = SomeModel()
model.trail(training_data)

model_path = some desired path
model.serialize(model, model_path)
```

Deploy your model, using your model-container:

```python
model_name = "example_model" # or some other model name
version = 1 # or some other version
container_name = "your_username/my_container:latest"
input_type = the input type your model accepts

clipper.deploy_model(
    model_name,
    version,
    model_path,
    "your_username/my_container:latest,
    ["example label 1", "example label 2"],
    input_type)
```

You can now go to your command line and run `docker ps --all` to see all containers you have started. Identify the (most recently created) container with the image corresponding to your model-container and grab its id, `<container_id>`. Take a look at its logs by running `docker logs <container_id>`. If all was successful, your container should still be running and the latest in the logs should read `Serving predictions for doubles.` If your container crashed or did not reach this point, use the logs to debug.

## Query your model

Register an application that to query your model with:

```python
default_output = some string
slo_micros = some integer time delay in microseconds (make this large)
app_name = some app name

clipper.register_application(
    app_name,
    model_name,
    input_type,
    default_output,
    slo_micros)
```

Use the Clipper Query Frontend API to query your model.

If the results of the API call are not what you'd expect (if, for instance, it looks like your model wasn't queried), check your model-container logs using `docker logs <container_id>`. 
