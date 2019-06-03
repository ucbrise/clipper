#!/usr/bin/env bash
#/bin/sh

docker run -p 8000:8000  prediction:container1  &
docker run -p 9001:9001  prediction:container2  &
docker run -p 11000:11000  prediction:container3  &
docker run -p 12000:12000  prediction:container4  &
docker run -p 13000:13000  prediction:container5  &
docker run -p 15000:15000  prediction:container7  &
docker run -p 16000:16000  prediction:container8  &
docker run -p 17000:17000  prediction:container9  &
docker run -p 18000:18000  prediction:container10  &
docker run -p 19000:19000  prediction:container11  &

