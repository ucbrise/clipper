#!/bin/bash

cd /container/data/dataset3/recordings

index=0;
for name in *.mp3
do
    ffmpeg -i "${name}" "${index}.wav"
    index=$((index+1))
    if [ $index -gt 1000 ]; then
      break
    fi
done

ls *.wav
echo "$(ls *.wav | wc -l) wav files in this dataset."
# 1001 wav files