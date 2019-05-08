#!/usr/bin/env bash
#/bin/sh

docker run -p 8000:8000 -it detection:container1
docker run -p 9000:9000 -it detection:container2
docker run -p 11000:11000 -it detection:container3
docker run -p 12000:12000 -it detection:container4
