#!/bin/bash
# The following line is crucial, otherwise when called in Dockerfile, pwd still gives "/"
cd /container/data/dataset1
unzip cmu_us_awb_arctic-0.95-release.zip

cd /container/data/dataset1/cmu_us_awb_arctic/wav
echo "$(ls *.wav | wc -l) wav files in /container/data/dataset1/cmu_us_awb_arctic." 
# 1138 wav files