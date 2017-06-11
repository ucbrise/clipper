# RPython model container 

This container enables the deployment of R models. It supports both R and Python runtime. It is to be noted that we can call R from Python using RPy2 interface. Therefore, this interface simplifies the task as it allows us to use the already written RPC client in python i.e.  <https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py>.


##Prerequisites :

In addition to the requirements of running clipper, 

1. R must be installed (version:latest , >=3.4)
2. Python must be installed. (version >=2.7) 
3. RPy2 must be installed. For more info. Got to <https://rpy2.bitbucket.io/>

