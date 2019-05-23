#!/bin/bash
# The following line is crucial, otherwise when called in Dockerfile, pwd still gives "/"
cd /container/data
unzip cmu_us_awb_arctic-0.95-release.zip