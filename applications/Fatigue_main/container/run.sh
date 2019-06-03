#!/usr/bin/env bash
#/bin/sh

docker run --runtime=nvidia -p 10000:10000 -it detection_main:raft
