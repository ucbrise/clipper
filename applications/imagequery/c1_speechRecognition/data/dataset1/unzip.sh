#!/bin/bash
# The following line is crucial, otherwise when called in Dockerfile, pwd still gives "/"
cd /container/data/dataset1
unzip cmu_us_awb_arctic-0.95-release.zip

echo "$(ls *.wav | wc -l) wav files in this dataset."