#/bin/bash

docker build -f ./Dockerfile0 -t imagequery_echo_model:container0 .
docker build -f ./Dockerfile1 -t imagequery_echo_model:container1 .
docker build -f ./Dockerfile2 -t imagequery_echo_model:container2 .
docker build -f ./Dockerfile3 -t imagequery_echo_model:container3 .
docker build -f ./Dockerfile4 -t imagequery_echo_model:container4 .