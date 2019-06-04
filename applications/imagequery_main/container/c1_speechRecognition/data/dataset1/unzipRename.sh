#!/bin/bash
# The following line is crucial, otherwise when called in Dockerfile, pwd still gives "/"

cd /container/c1_speechRecognition/data/dataset1
unzip -qq cmu_us_awb_arctic-0.95-release.zip

# the unzipped file is /container/data/dataset1/cmu_us_awb_arctic
cd /container/c1_speechRecognition/data/dataset1/cmu_us_awb_arctic/wav
index=0;
for name in *.wav
do
    mv "${name}" "${index}.wav"
    index=$((index+1))
    if [ $index -gt 1000 ]; then
      break
    fi
done

echo "$(ls *.wav | wc -l) wav files in /container/c1_speechRecognition/data/dataset1/cmu_us_awb_arctic." 
# 1138 wav files