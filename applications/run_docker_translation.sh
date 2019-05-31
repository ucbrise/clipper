#!/usr/bin/env bash
#/bin/sh

docker run -p 8000:8000 --net=host -it translation:container1  &
docker run -p 9000:9000 --net=host -it translation:container2  &
docker run -p 11000:11000 --net=host -it translation:container3  &
docker run -p 12000:12000 --net=host -it translation:container4  &
