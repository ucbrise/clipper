#!/usr/bin/env bash
#/bin/sh

docker build -f ./prediction/Dockerfile1 -t zsxhku/stock:container1 .
docker build -f ./prediction/Dockerfile2 -t zsxhku/stock:container2 .
docker build -f ./prediction/Dockerfile3 -t zsxhku/stock:container3 .
docker build -f ./prediction/Dockerfile4 -t zsxhku/stock:container4 .
docker build -f ./prediction/Dockerfile5 -t zsxhku/stock:container5 .
docker build -f ./prediction/Dockerfile6 -t zsxhku/stock:container6 .
docker build -f ./prediction/Dockerfile7 -t zsxhku/stock:container7 .
docker build -f ./prediction/Dockerfile8 -t zsxhku/stock:container8 .
docker build -f ./prediction/Dockerfile9 -t zsxhku/stock:container9 .
docker build -f ./prediction/Dockerfile10 -t zsxhku/stock:container10 .
docker build -f ./prediction/Dockerfile11 -t zsxhku/stock:container11 .

# C:/Users/musicman/Desktop/financial_prediction
