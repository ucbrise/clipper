# RPython model container 

This container enables the deployment of R models. It supports both R and Python runtime. It is to be noted that we can call R from Python using RPy2 interface. Therefore, this interface simplifies the task as it allows us to use the already written RPC client in python i.e.  <https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py>.


## Prerequisites :

In addition to the requirements of running clipper, 

1. R must be installed (version:latest , >=3.4)
2. Python version must be >=2.7. 
3. RPy2 must be installed. For more info. go to <https://rpy2.bitbucket.io/>


## Instructions for Model Deployment and Prediction :


- Make sure  'r_python_container' image is created from <clipper-root>/RPythonDockerfile and Clipper is running.
- R models can be formulated and trained in python notebooks using RPy2 interface, for example :

```py
import rpy2.robjects as ro
ro.r('formula=mpg~wt+cyl') #model's formula
ro.r('dataset=mtcars')  #model's training dataset
model_RPy2 = ro.r('model_R <- lm(formula,data=dataset)') 
#model_RPy2 is RPy2 object referring to the R model

```
- An already trained and saved model (in .rds format) can also be loaded as RPy2 obeject :

```py
from rpy2.robjects.packages import importr
base = importr('base')
self.model = base.readRDS(PATH)  #PATH is the path of saved model.

```

- Deploy the model :

```py
Clipper.deploy_R_model(
   "example_model",1,model_RPy2,"strings"
   )
```

The input for prediction is of type string.These strings are basically pandas dataframes encoded as strings. 

- Register Application :

```
Clipper.register_application(
    "example_app","example_model","strings",default_output,slo_micros=2000
    )
 ```  
  
- Predict :

 For prediction, dataframes are to be converted to strings.
for example :

pandas_dataframe = DataFrame({'wt':[5.43,6.00,7.89],'cyl' :[4.32,5.76,7.90]})
should be converted to list of string:["wt;cyl\n5.43;4.32","wt;cyl\n6.00;5.76","wt;cy\n7.89;7.90"].

Then we can pass these strings through requests.post() method and get predictions for each string(i.e for each coloumn of dataframe).

Illustration can be found in predict_R_model() method of 
<clipper-root>/integration-tests/deploy_R_containers.py
