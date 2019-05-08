#!/usr/bin/env bash
#/bin/sh

docker build -f ./Dockerfile1 -t detection:container1 .
docker build -f ./Dockerfile2 -t detection:container2 .
docker build -f ./Dockerfile3 -t detection:container3 .
docker build -f ./Dockerfile4 -t detection:container4 .

