#!/usr/bin/env bash
#/bin/sh

docker build -f ./translation/Dockerfile4 -t translation:container4 .
docker build -f ./translation/Dockerfile1 -t translation:container1 .

