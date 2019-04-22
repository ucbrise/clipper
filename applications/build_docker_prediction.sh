#!/usr/bin/env bash
#/bin/sh

docker build -f ./prediction/Dockerfile1 -t prediction:container1 .
docker build -f ./prediction/Dockerfile2 -t prediction:container2 .
docker build -f ./prediction/Dockerfile3 -t prediction:container3 .
docker build -f ./prediction/Dockerfile4 -t prediction:container4 .
docker build -f ./prediction/Dockerfile5 -t prediction:container5 .
docker build -f ./prediction/Dockerfile7 -t prediction:container7 .
docker build -f ./prediction/Dockerfile8 -t prediction:container8 .
docker build -f ./prediction/Dockerfile9 -t prediction:container9 .
docker build -f ./prediction/Dockerfile10 -t prediction:container10 .

# C:/Users/musicman/Desktop/financial_prediction
