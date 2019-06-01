# Boat
A fault-tolerant prediction serving system based on Clipper and Raft

## Introduction
Prediction serving systems for machine learning like [Clipper](https://clipper.ai) simplifies the deployment of machine learning models. However, most of them, if not all, do not guarantee high availability. The entire pipeline fails when one of the models crashes in the middle. 

Boat leverages state machine replication (SMR) based on Raft to provide redundancy. Prediction requests are replicated so that when an instance of prediction serving system failes, the requests can be replayed so that the state of that instance can be restored. In this project, Clipper and a modified version of RaftOS is used. Boat is named after the object category that contains both rafts and clippers.

## Setup
### Prerequisites  
In this setup guide, we assume you have met the following prerequisites. While other versions might work as well, these are what we test Boat on, thus provide best luck. We also assume that your `pwd` is the root of this repository.
- Ubuntu 18.04.2 LTS (Windows doesn't do)
- Python 3.6.3 (with python3-pip installed)
- Docker 18.09 (current user must be able to execute `docker` command without `sudo`)

### Dependencies
**PyPI Packages**

See [requirements.txt](https://github.com/jitaogithub/boat/blob/master/requirements.txt). Can be installed in batch with:

`sudo pip3 install -r requirements.txt`

**Clipper**  

You can either copy the `clipper_admin` folder into `./` from [Clipper's GitHub repository](https://github.com/ucbrise/clipper) or install with a single command:

`sudo pip3 install git+https://github.com/ucbrise/clipper.git@develop#subdirectory=clipper_admin`

**RaftOS**  

There is no need to install RaftOS. This repository contains a modified version.

## Quickstart
To start, simply execute:

`python3 deploy.py`

The first run might take several minutes. 

By default, this will run three Boat instances. Each Boat instance has a frontend that provides API for users to manipulate:

`curl -X GET 127.0.0.1:8080` 

Default ports are 8080, 8081 and 8082 for each instance respectively.

This gives a greeting message in plain text. The API also allows checking the status of the instance:

`curl -X GET 127.0.0.1:8080/status`

The response will be a JSON string that looks like:

`{ "is_leader": true }`

`"is_leader"` tells whether this Boat instance is the Raft leader. In case you don't know about Raft, this is basically saying whether this instance can accept your predict request. You should always submit your request to the leader.

To submit a predict request, simply POST to the leader (the instance listening to 8082 for example):

`curl -X POST -d '{ "input": "Hello" }' --header "Content-Type:application/json" 127.0.0.1:8082/predict`

By default, Boat runs a model that does nothing but echoes the input. However, the result will not be responded to you. In the console that runs Boat, you may see something like:

```
19-05-08:21:58:19 INFO     [boat.py:100] Boat 1: Request received from raft: {'time': '2019-05-08 21:58:19.668935', 'request': '{ "input": "Hello" }'}
19-05-08:21:58:19 INFO     [boat.py:100] Boat 0: Request received from raft: {'time': '2019-05-08 21:58:19.668935', 'request': '{ "input": "Hello" }'}
19-05-08:21:58:19 INFO     [boat.py:85] Boat 2: Request from API appended to state: {'time': '2019-05-08 21:58:19.668935', 'request': '{ "input": "Hello" }'}
19-05-08:21:58:19 INFO     [boat.py:115] Boat 0: { "input": "Hello" } sent to clipper with status 200 and response:
        {"query_id":0,"output":"b'Hello'","default":false}
19-05-08:21:58:19 INFO     [boat.py:115] Boat 1: { "input": "Hello" } sent to clipper with status 200 and response:
        {"query_id":0,"output":"b'Hello'","default":false}
19-05-08:21:58:19 INFO     [boat.py:115] Boat 2: { "input": "Hello" } sent to clipper with status 200 and response:
        {"query_id":0,"output":"b'Hello'","default":false}
```

To put it simple, the frontend of the leader replicates the request and synchronize with followers (first three lines). After that, the request is forwarded to the Clipper backend to get the response JSON string(last three lines).

To quit, send `ctrl-c` to the console. Boat will automatially clean up all the docker containers, networks and images that Clipper usually leaves. If this is somehow not aborted, manually execute:

```shell
chmod u+x reset.sh
# Need to specify which Clipper instance to reset
./reset.sh NODE_ID
```

## Advanced Application
Each module in Boat is highly cohesive. Clipper is essentially independent from Raft. To deploy complex models, you may modify `clipper.py` following the [documentation of Clipper](http://docs.clipper.ai/en/latest/model_deployers.html).

## Failure and recovery
Boat instances automatically restarts on failure. The requests will be automatically synchronized from living nodes and replayed. For evaluation purposes, an API is implemented for active failure of nodes. For example, to fail Boat 2, simply execute:

`curl -X POST -d '{ "cmd": "exit" }' --header "Content-Type:application/json" 127.0.0.1:8082/control`

It will restart after finishing the clean-up.