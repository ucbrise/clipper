#!/usr/bin/env bash
#/bin/sh

docker run -p 8000:8000 -it translation:container1 . &
docker run -p 9000:9000 -it translation:container1 . &
docker run -p 11000:11000 -it translation:container1 . &
