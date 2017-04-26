# Python prediction function deployment
Clipper supports a simplified process for deploying arbitrary Python prediction functions as model-containers. 

The Clipper management library supports this functionality through `deploy_prediction_func`, which consumes a the function you want to deploy. All dependencies for the function must be installed with Anaconda or Pip. `deploy_prediction_func` must be called from within an [Anaconda environment](https://conda.io/docs/using/envs.html) that tracks all such dependencies.

## Defining your prediction function
Prediction functions should take a list of inputs of and return a Python or numpy array of predictions. Internally, the function is free to do anything so long as the function's closure captures its full state.

  
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
      
## Deploying your prediction function
Deploying this function to Clipper doesn't require writing any container code. All that's needed is a call to `deploy_predict_function` from within an Anaconda environment that has all necessary conda and pip packages to run the function.

    clipper.deploy_predict_function(
      "example_model",
      1,
      centered_predict,
      ["example"],
      "doubles",
      num_containers=1)

The management library will do some dependency checks from your client before deploying the function.

It will first ensure that no packages have conflicting dependencies in the container's linux-64 conda channel. If they do, you will see a message like:

  Fetching package metadata .........
  Solving package specifications: ..........
  Your conda dependencies are unsatisfiable (see error text below). Please resolve these issues and call `deploy_predict_func` again.
  UnsatisfiableError: The following specifications were found to be in conflict:
    - appnope 0.1.0 py27_0
    - scikit-learn 0.17.1 np110py27_0 -> mkl 11.3.1
      - scikit-learn 0.17.1 np110py27_0 -> numpy 1.10*




If none of your packages have conflicting dependencies, the management library will check for the existence of your packages in the linux-64 conda channel. The deployment will skip any that do not. You should see a message like this:

```
Fetching package metadata .........
Solving package specifications: ..........
The following packages in your conda environment aren't available in the linux-64 conda channel the container will use:
conda-forge::nltk 3.2.1 py27_0, conda-forge::libsodium 1.0.10 0, conda-forge::cloudpickle 0.2.1 py27_0
...
We will skip their installation when deploying your function. If your function uses these packages, the container will experience a runtime error when queried.
Removed unavailable packages from supplied environment specifications
```

A successful call to `deploy_prediction_function` will output

  Supplied environment details
  Serialized and supplied predict function
  Published model to Clipper
  Found clipper/python-container in Docker hub
  Copied model data to host
  True

## Confirming that your model-container is ready
Calling `deploy_prediction_func` spins up a container and loads it with the necessary info to create your model. However, it may take some time before the model-container is ready to serve predictions: it still has to install and load dependencies and deserialize your prediction function.

You can read the container's logs to keep track of its initialization progress. Run `docker ps` and find the container instance for the image `clipper/python-container `. Grab its container id and run `docker logs <container_id>`.

There are several stages in the model-container's initialization.

**1. Loading Anaconda packages**

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

**2. Loading pip packages**

If you still have pip packages to install, the output of `docker logs` should show progress like so:

```
...
Collecting packaging==16.8
  Downloading packaging-16.8-py2.py3-none-any.whl
Collecting path-and-address==2.0.1
  Downloading path-and-address-2.0.1.zip
...
```

**3. Starting up the model in the newly created Anaconda environment.**

In this stage, the container attempts to load in the parameters you have provided it and connect to Clipper accordingly. If all is correct, the output of `docker logs` should show:

```
Starting PythonContainer container
Connecting to Clipper with <port information>
```

**4. Loading your prediction function.**
In this stage, the container deserializes your prediction function and embeds in the model so it can be queried. The start of this process is indicated by

```
Loading provided Python prediction function
```

and if it completes without errors, you should see

```
Serving predictions for <input_type> input type.
```

At this point, your prediction function and its environment have successfully been loaded and are ready to be queried!

## Errors you can encounter

TODO
