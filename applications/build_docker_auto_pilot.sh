#/bin/bash

docker build -f ./auto_pilot/Dockerfile1 -t auto_pilot:container1 .
docker build -f ./auto_pilot/Dockerfile2 -t auto_pilot:container2 .
docker build -f ./auto_pilot/Dockerfile3 -t auto_pilot:container3 .
docker build -f ./auto_pilot/Dockerfile4 -t auto_pilot:container4 .
docker build -f ./auto_pilot/Dockerfile5 -t auto_pilot:container5 .