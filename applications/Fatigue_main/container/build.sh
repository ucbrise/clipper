#!/usr/bin/env bash
#/bin/sh
if [ ! -e ./part1 ];
then
chmod 777 dataDownLoad.sh
./dataDownLoad.sh
tar -xvf part1.tar.gz
fi
if [ ! -f ./container2/app/]
docker build -f ./Dockerfile_main -t FatigueDetection_main:raft .


