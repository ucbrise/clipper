#!/usr/bin/env bash
#/bin/sh

docker run -p 8000:8000 -it imagequery:container1  &
docker run --runtime=nvidia -p 9000:9000 -it imagequery:container2  &
docker run -p 11000:11000 -it imagequery:container3  &
docker run -p 12000:12000 -it imagequery:container4  &


# in detached mode
docker run -d -p 8000:8000 imagequery:container1  &
docker run --runtime=nvidia -d -p 9000:9000 imagequery:container2  &
docker run -d -p 11000:11000 imagequery:container3  &
docker run -d -p 12000:12000 imagequery:container4  &
