#!/usr/bin/env bash
#/bin/sh

docker build -f ./FatigueDetection/Dockerfile1 -t detection:container1 .
docker build -f ./FatigueDetection/Dockerfile2 -t detection:container2 .
docker build -f ./FatigueDetection/Dockerfile3 -t detection:container3 .
docker build -f ./FatigueDetection/Dockerfile4 -t detection:container4 .

