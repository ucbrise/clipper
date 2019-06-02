#/bin/bash

docker build -f ./imagequery_echo_model/Dockerfile0 -t imagequery_echo_model:container0 .
docker build -f ./imagequery_echo_model/Dockerfile1 -t imagequery_echo_model:container1 .
docker build -f ./imagequery_echo_model/Dockerfile2 -t imagequery_echo_model:container2 .
docker build -f ./imagequery_echo_model/Dockerfile3 -t imagequery_echo_model:container3 .
docker build -f ./imagequery_echo_model/Dockerfile4 -t imagequery_echo_model:container4 .