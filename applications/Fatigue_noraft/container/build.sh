#!/usr/bin/env bash
#/bin/sh
./dataDownLoad.sh
tar -xvf part1.tar.gz
docker build -f ./Dockerfile_main -t FatigueDetection_main:noraft .


