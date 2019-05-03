#Standalone Test

##Step 1: Run Development Docker 
```sh
docker run -it --network=host -v [YOUR_CODE_PATH_TO_CLIPPER]:/clipper -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp zsxhku/clipper_test:version1
```

##Step 2: Go to clipper_admin dir

```sh
cd /clipper/clipper_admin
```
##Step 3: Start DAG deployment

###Test case 1: simple dag (No Prediction)
```sh
python simple_dag.py
```

###Test case 2: predict stock price
```sh
python stock.py
```
##Step 4: See the dockers/logs
```sh
docker container ls 
docker container [CONTAINER_ID]
```
##Step 5: Stock DAG input / request
```sh
docker run -it --network clipper_network zsxhku/grpcclient [IP_OF_THE_ENTRY_PROXY] 22223
```
You can see the /grpcclient/app/grpc_client.py and see the implementations and implement you own grpcclient docker

But remember you should run the grpcclient docker under clipper_network


##Step 5: Stop containers
```sh
python stop_all.py
```

#Build your own application DAG and deployment 






