# RPython model container 
This container enables the deployment of R models. It supports both the R and Python runtimes. R functions are called from Python using the RPy2 interface, while the container is built upon the [Python RPC client](https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py).

## Prerequisites :
In addition to the requirements of running clipper, 

1. R must be installed (version:latest , >=3.4)
2. Python version must be =2.7. 
3. RPy2 must be installed. For more info, go to <https://rpy2.bitbucket.io/>

## Instructions for Model Deployment and Prediction :
- Make sure  'r_python_container' image is created from <clipper-root>/RPythonDockerfile and Clipper is running.
- R models can be formulated and trained in python notebooks using the RPy2 interface, for example :

```py
import rpy2.robjects as ro
ro.r('formula=mpg~wt+cyl') #model's formula
ro.r('dataset=mtcars')  #model's training dataset
# Create an R model with an RPy2 reference
model_RPy2 = ro.r('model_R <- lm(formula,data=dataset)') 
```
- A previously trained and saved model (in .rds format) can also be loaded as RPy2 object :

```py
from rpy2.robjects.packages import importr
base = importr('base')
self.model = base.readRDS(PATH)  #PATH is the path of saved model.
```

- Deploy the model :

```py
Clipper.deploy_R_model(
   "example_model", 1, model_RPy2
   )
```

- Register Application :

```py
Clipper.register_application(
    "example_app", "strings", default_output, slo_micros=2000
    )
 ```
- Link model to application :

```py
Clipper.link_model_to_app(
		"example_app", "example_model"
		)
```

- Requesting predictions

The container inputs are of type **string**. Each string input is a csv-encoded pandas dataframe. 
The following is an example encoding:
```py
   pandas_dataframe = DataFrame({'wt':[5.43,6.00,7.89],'cyl' :[4.32,5.76,7.90]})
   encoded_dataframe = pandas_dataframe.to_csv(sep=";")
   assert type(encoded_dataframe) == str
```

Once a data frame has been string encoded, we can pass it to the container via the `requests.post()` method and obtain batched predictions for each data frame row.

This process is illustrated in the `predict_R_model()` method of 
[R Model Integration Test](../../integration-tests/deploy_R_models.py)
