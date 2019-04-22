#!/usr/bin/env bash
#/bin/sh

docker run -p 8000:8000 -it prediction:container1  &
docker run -p 9000:9000 -it prediction:container2  &
docker run -p 11000:11000 -it prediction:container3  &
docker run -p 12000:12000 -it prediction:container4  &
docker run -p 13000:13000 -it prediction:container5  &
docker run -p 15000:15000 -it prediction:container7  &
docker run -p 16000:16000 -it prediction:container8  &
docker run -p 17000:17000 -it prediction:container9  &
docker run -p 18000:18000 -it prediction:container10  &
docker run -p 19000:19000 -it prediction:container11  &

