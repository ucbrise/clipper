# Python prediction function deployment
Clipper supports a simplified process for deploying arbitrary Python prediction functions as model-containers. 

The Clipper management library supports this functionality through `deploy_prediction_func`, which consumes a function you want to deploy. All dependencies for the function must be installed with Anaconda or Pip. `deploy_prediction_func` must be called from within an [Anaconda environment](https://conda.io/docs/using/envs.html) that tracks all such dependencies.

## Defining your prediction function
Prediction functions should take a list of inputs of type `<input-type>` and return a Python list of strings (often JSON strings, but JSON is not required). Internally, the function is free to do anything so long as the function's closure captures its full state.

```py
from sklearn import linear_model

def center(xs):
  means = np.mean(xs, axis=0)
  return xs - means

centered_xs = center(xs)
model = sklearn.linear_model.LogisticRegression()
model.fit(centered_xs, ys)

def centered_predict(inputs):
  centered_inputs = center(inputs)
  return model.predict(centered_inputs)

```
      
## Requesting deployment of  your Python function
Deploying this function to Clipper doesn't require writing any container code. All that's needed is a call to `deploy_predict_function()` from within an Anaconda environment that has necessary conda and pip packages to run the function.

```py
clipper.deploy_predict_function(
  "example_model",
  1,
  centered_predict,
  ["example"],
  "doubles",
  num_containers=1)
```

The management library will do some dependency checks from your client before deploying the function:

###1. Checking for conflicting package dependencies

The management library will first ensure that no packages have conflicting dependencies in the container's linux-64 conda channel. If they do, you will see a message like:

    Fetching package metadata .........
    Solving package specifications: ..........
    Your conda dependencies are unsatisfiable (see error text below). Please resolve these issues and call `deploy_predict_func` again.
    UnsatisfiableError: The following specifications were found to be in conflict:
      - appnope 0.1.0 py27_0
      - scikit-learn 0.17.1 np110py27_0 -> mkl 11.3.1
      - scikit-learn 0.17.1 np110py27_0 -> numpy 1.10*


###2. Checking for package existence in linux channels

If none of your packages have conflicting dependencies, the management library will check for the existence of your packages in the linux-64 conda channel (the channel the container will use). If any packages cannot be found, they will be skipped during package installation on the container. You should see a message like this:

```
Fetching package metadata .........
Solving package specifications: ..........
The following packages in your conda environment aren't available in the linux-64 conda channel the container will use:
conda-forge::nltk 3.2.1 py27_0, conda-forge::libsodium 1.0.10 0, conda-forge::cloudpickle 0.2.1 py27_0
...
We will skip their installation when deploying your function. If your function uses these packages, the container will experience a runtime error when queried.
Removed unavailable packages from supplied environment specifications
```

###3. Deploying the function in a container

A successful call to `deploy_prediction_function()` will output:

    Supplied environment details
    Serialized and supplied predict function
    Published model to Clipper
    Found clipper/python-container in Docker hub
    Copied model data to host
    True

## Initializing your model-container
Calling `deploy_prediction_func()` starts a Docker container and loads it with the necessary info to create your model. However, it may take some time (up to several minutes) before the model-container is ready to serve predictions: it still has to install and load dependencies and deserialize your prediction function.

#### How you can check container logs
You can read the container's logs to keep track of its initialization progress. Run `docker ps` and find the container instance for the image `clipper/python-container `. Grab its container id and run `docker logs <container_id>`.

There are several stages in the model-container's initialization.
#### 0. Optimistic Startup
The container will attempt to start up the model without downloading any packages. The start of this process is reflected in the output of `docker logs` as:

  Attempting to run Python container without installing dependencies
  
If this attempt is successful, the logs should show the following when the container is ready to be queried:

  Starting PythonContainer container
  Connecting to Clipper with <port information>
  Initializing Python function container
  Serving predictions for <input_type> input type.

**If an ImportError is reached** either at startup or upon being queried, the logs will show:
  
  Running Python container without installing dependencies fails
  Will install dependencies and try again

and the process will continue on to the steps detailed below.

**If any other error is reached** the logs will show the error and the process will not continue (installing dependencies could only possibly resolve ImportErrors).  

#### 1. Loading Anaconda packages

If you still have conda packages to install, the output of `docker logs` should show progress like so:

```
...
bokeh-0.12.5-p 100% |###############################| Time: 0:00:02   1.88 MB/s
boto3-1.4.4-py 100% |###############################| Time: 0:00:00  12.64 MB/s
fabric-1.13.1- 100% |###############################| Time: 0:00:00   4.76 MB/s
flask-cors-2.1 100% |###############################| Time: 0:00:00 347.15 kB/s
s3fs-0.0.9-py2 100% |###############################| Time: 0:00:00  12.35 MB/s
blaze-0.10.1-p 100% |###############################| Time: 0:00:00   4.79 MB/s
...
```

### 2. Loading pip packages

If you still have pip packages to install, the output of `docker logs` should show progress like so:

```
...
Collecting packaging==16.8
  Downloading packaging-16.8-py2.py3-none-any.whl
Collecting path-and-address==2.0.1
  Downloading path-and-address-2.0.1.zip
...
```

### 3. Starting up the model

In this stage, the container attempts to load in the parameters you have provided it and connect to Clipper accordingly. The output of `docker logs` should show:

```
Starting PythonContainer container
Connecting to Clipper with <port information>
```

### 4. Loading your prediction function
In this stage, the container deserializes your prediction function and embeds in the model so it can be queried. The start of this process is indicated by:

```
Loading provided Python prediction function
```

### 5. Ready to query!
After your prediction function has been loaded, you should see:

```
Serving predictions for <input_type> input type.
```

At this point, your prediction function and its environment have successfully been loaded and are ready to be queried!

## Errors you can encounter

There are several problems you can encounter in deploying your Python function to Clipper. This section will walk you through how you can identify and resolve them.

If you run into issues not listed here, please file a GitHub issue and we will try to help you diagnose and solve the problem.

### Errors during container initialization
**Some pip packages weren't installed**

This is possible if conda is not tracking your pip packages. Make sure that you install your pip packages through a conda-installed `pip`. This way, conda can keep track of which pip packages you are using in your environment. You can see which pip packages conda is keeping track of by writing your environment to a file with `conda env export >> <filename>`

**Logs indicate that some pip packages don't exist in the container's pip distribution library**

If this occurs, `docker logs` should output something like the following during the 'Loading pip packages' phase:
    
  Collecting nb-conda-kernels==2.1.0
    Could not find a version that satisfies the requirement nb-conda-kernels==2.1.0 (from versions: )
  No matching distribution found for nb-conda-kernels==2.1.0

  CondaValueError: Value error: pip returned an error.
  
This will not stop container startup; the pip package's installation will be skipped. If your Python function needs the package, you can run into runtime errors upon querying the container. The best way to avoid this issue is to ensure that all of your pip packages exist on linux-64 pip distribution library.

### Errors at query time
Runtime errors that occur within the container will crash it. You can diagnose what caused the issue by looking at the out put of `docker logs <container_id>` (where `<container_id>` is the id of the container in which your Python function was deployed)

**Container crash: module not found**

This can occur if:
 
* Your function makes use of a pip package that your conda environment wasn't tracking. See the "Some pip packages weren't installed" section above.
* Your function makes use of a conda package that doesn't exist in the container's conda channel and whose installation was skipped. If any packages in your local conda environment were skipped for installation on the container, you should have been notified of them in a warning message after calling `deploy_predict_function`.
* Conda and pip both don’t have a dependency being used by the function. Unfortunately, Clipper's Python function deployment feature only supports conda and pip packages for now. Usage of modules not within these distributions is incompatible with this deployment process.

**Container crash: prediction function has some other runtime error (index out of bounds, divide by zero, incorrect operand types for a function, etc**

Make sure that you test your function locally before deploying it. Remember that it should take as input a list of datapoints (numpy arrays or native Python lists) with entries of type <input_type> and should output a list of strings with a prediction for each datapoint.

**Container crash: function makes use of something that’s not captured in the function closure**

Your function can't use objects or connections whose states aren't captured in the function closure. An example of this would be a database connection that is configured/initialized outside of the function.

**Container logs indicate no issues but the Python function's predictions aren't getting served**
First, make sure that your Python function is actually getting called. One way to do this is to check the container's logs for print statements in your Python function.

If you can confirm that your function is indeed getting called, then thereare a couple of things that could prevent its prediction from getting served:

* Prediction function returns items of the wrong shape
* Prediction function takes too long (longer than the `latency_slo_micros` parameter of the app) and so its results are never considered
