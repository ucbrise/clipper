#!/usr/bin/env bash
#/bin/sh

docker run -p 7000:7000 -it imagequery:container0 & 
docker run --runtime=nvidia -e MODEL_NAME=test -e MODEL_PORT=22222 -p 8000:8000 -it imagequery:container1  &
docker run --runtime=nvidia -e MODEL_NAME=test -e MODEL_PORT=22222 -p 9000:9000 -it imagequery:container2 &
docker run -p 11000:11000 -it imagequery:container3  &
docker run -p 12000:12000 -it imagequery:container4  &

