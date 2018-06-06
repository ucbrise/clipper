Rclipper
--------

Rclipper is a package for building serveable Clipper models from R
functions. Given an API-compatible R function, Rclipper’s
**build\_model** function builds a Docker image for a Clipper model.
This model can then be deployed to Clipper via the [Python
clipper\_admin package](https://pypi.python.org/pypi/clipper_admin).

Dependencies
------------

Rclipper depends on the [Python clipper\_admin
package](https://pypi.python.org/pypi/clipper_admin) for building and
deploying models. In order to use this admin package, [Docker for
Python](https://pypi.python.org/pypi/docker/) must also be installed.

Importing Rclipper
------------------

**It is very important that Rclipper be imported before a prediction
function or its dependencies are defined**. Rclipper makes use of the
[histry
package](https://cran.r-project.org/web/packages/histry/index.html) to
statically analyze dependency definitions. In order to locate these
definition expressions during function serialization, histry must be
imported before the expressions are executed.

Writing an API-compatible R prediction function
-----------------------------------------------

An API-compatible prediction function must accept a type-homogeneous
**list** of inputs of one of the following types:

-   Raw Vector
-   Integer Vector
-   Numeric Vector
-   String (length-1 character vector)
-   Data Frame
-   Matrix
-   Array
-   List

Additionally, **given a list of inputs** of length *N*, a prediction
function **must return a list of outputs** of length *N*. All elements
of the output list must be of the same type.

**Note:** If a prediction function returns a list of string (length-1
character vector) objects, each output will be returned as-is, without
any additional serialization. Otherwise, all non-string outputs will be
string-serialized via the [jsonlite
package](https://cran.r-project.org/web/packages/jsonlite/index.html),
and their serialized representations will be returned.

Building a model
----------------

Once you’ve written an API-compatible prediction function, you can build
a Clipper model with it via the **build\_model** function:

    #' @param model_name character vector of length 1. The name to assign to the model image.
    #' @param model_version character vector of length 1. The version tag to assign to the model image.
    #' @param prediction_function function. This should accept a type-homogeneous list of 
    #' inputs and return a list of outputs of the same length. If the elements of the output list
    #' are not character vectors of length 1, they will be converted to a serialized
    #' string representation via 'jsonlite'.
    #' @param sample_input For a prediction function that accepts a list of inputs of type X,
    #' this should be a single input of type X. This is used to validate the compatability
    #' of the function with Clipper and to determine the Clipper data type (bytes, ints, strings, etc)
    #' to associate with the model.
    #' @param model_registry character vector of length 1. The name of the image registry
    #' to which to upload the model image. If NULL, the image will not be uploaded to a registry.

    Rclipper::build_model(model_name, model_version, prediction_function, sample_input, model_registry = NULL)

This will build a Docker image with the tag:
*model\_registry*/*model\_name*:*model\_version*. If no registry was
specified, the image will have the tag: *model\_name*:*model\_version*.
Additonally, this function will output a command that you can execute
within an interactive Python environment to deploy the model with the
[clipper\_admin package](https://pypi.python.org/pypi/clipper_admin).

Deploying a model
-----------------

Once you’ve built a model, use the provided command to deploy it with
the [clipper\_admin
package](https://pypi.python.org/pypi/clipper_admin). For information
about how to register the model with an application so that it can be
queried, please consult the [clipper\_admin API
documentation](http://docs.clipper.ai/en/).

Querying a model
----------------

After you’ve built a model, deployed the model, and registered the model
with an application, you can query it with input data of the correct
type. The following table maps the input type of your model’s prediction
function to the Clipper input type associated with your deployed model:

<table>
<thead>
<tr class="header">
<th style="text-align: right;">R input type</th>
<th style="text-align: left;">Clipper Input Type</th>
<th style="text-align: center;">JSON Format</th>
<th style="text-align: center;">Example</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td style="text-align: right;">Raw Vector</td>
<td style="text-align: left;">Bytes</td>
<td style="text-align: center;">Base64-encoded string</td>
<td style="text-align: center;">Y2xpcHBlciB0ZXh0</td>
</tr>
<tr class="even">
<td style="text-align: right;">Integer Vector</td>
<td style="text-align: left;">Ints</td>
<td style="text-align: center;">Integer array</td>
<td style="text-align: center;">[1,2,3,4]</td>
</tr>
<tr class="odd">
<td style="text-align: right;">Numeric Vector</td>
<td style="text-align: left;">Doubles</td>
<td style="text-align: center;">Floating point array</td>
<td style="text-align: center;">[1.0,2.0,3.0.,4.0]</td>
</tr>
<tr class="even">
<td style="text-align: right;">Character Vector</td>
<td style="text-align: left;">Strings</td>
<td style="text-align: center;">String</td>
<td style="text-align: center;">“input text”</td>
</tr>
<tr class="odd">
<td style="text-align: right;">Data Frame</td>
<td style="text-align: left;">Strings</td>
<td style="text-align: center;">String</td>
<td style="text-align: center;"><code>jsonlite::toJSON(mtcars)</code></td>
</tr>
<tr class="even">
<td style="text-align: right;">Matrix</td>
<td style="text-align: left;">Strings</td>
<td style="text-align: center;">String</td>
<td style="text-align: center;"><code>jsonlite::toJSON(diag(3))</code></td>
</tr>
<tr class="odd">
<td style="text-align: right;">Array</td>
<td style="text-align: left;">Strings</td>
<td style="text-align: center;">String</td>
<td style="text-align: center;"><code>jsonlite::toJSON(array(1:4))</code></td>
</tr>
<tr class="even">
<td style="text-align: right;">List</td>
<td style="text-align: left;">Strings</td>
<td style="text-align: center;">String</td>
<td style="text-align: center;"><code>jsonlite::toJSON(list(1:4))</code></td>
</tr>
</tbody>
</table>

Example
-------

### Import Rclipper

    library(Rclipper)

    ## Loading required package: CodeDepends

    ## Loading required package: histry

### Define an API-compatible prediction function

    #' Given a list of vector inputs,
    #' outputs a list containing the
    #' length of each input vector as a string
    pred_fn = function(inputs) {
    return(lapply(inputs, function(input) {
    return(as.character(length(input)))
    }))
    }

    print(pred_fn(list(c(1,2), c(3))))

    ## [[1]]
    ## [1] "2"
    ## 
    ## [[2]]
    ## [1] "1"

### Build a model

    # Specify that the prediction function expects integer vectors
    # by supplying an integer vector as the sample input
    Rclipper::build_model("test-model", "1", pred_fn, sample_input = as.integer(c(1,2,3)))

    ## [1] "Serialized list of dependent libraries: Rclipper: knitr: histry: CodeDepends: stats: graphics: grDevices: utils: datasets: methods: base"
    ## [1] "Serialized model function!"
    ## [1] "Done!"
    ## To deploy this model, execute the following command from a connected ClipperConnection object `conn`:
    ## conn.deploy_model("test-model", "1", "ints", "test-model:1", num_replicas=<num_container_replicas>)

### Deploy and link the model

This assumes that a Clipper cluster is running on *localhost* with a
registered application that has the name *app1*. In a Python interactive
environment:

    from clipper_admin import DockerContainerManager, ClipperConnection
    cm = DockerContainerManager()
    conn = ClipperConnection(cm)
    conn.connect()

    # Deploy a single replica of the model
    conn.deploy_model(name="test-model", version="1", input_type="ints", image="test-model:1", replicas=1)

    conn.link_model_to_app(app_name="app1", model_name="test-model")

### Query the model

You can now query the model from any HTTP client. For example, directly
from the command line with [cURL](https://github.com/curl/curl):

    $ curl -X POST --header "Content-Type:application/json" -d '{"input": [1,2,3,4]}' 127.0.0.1:1337/app1/predict

    $ {"query_id":2,"output":4,"default":false}
