#/bin/bash

docker build -f ./imagequery/Dockerfile1 -t imagequery:container1 .
docker build -f ./imagequery/Dockerfile2 -t imagequery:container2 .
docker build -f ./imagequery/Dockerfile3 -t imagequery:container3 .