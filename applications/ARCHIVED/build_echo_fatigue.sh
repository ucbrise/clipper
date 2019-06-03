#!/usr/bin/env bash
#/bin/sh


docker build -f Fatigue_echo/Dockerfile0 -t detection_echo:container0 .
docker build -f Fatigue_echo/Dockerfile1 -t detection_echo:container1 .
docker build -f Fatigue_echo/Dockerfile2 -t detection_echo:container2 .
docker build -f Fatigue_echo/Dockerfile3 -t detection_echo:container3 .
docker build -f Fatigue_echo/Dockerfile4 -t detection_echo:container4 .
docker build -f Fatigue_echo/Dockerfile5 -t detection_echo:container5 .

