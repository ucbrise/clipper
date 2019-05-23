#!/bin/bash

cd /container/im2txt/data/imageDataset3/speech-accent-archive/recordings

index=0;
for name in *.wav
do
    mv "${name}" "${index}.wav"
    index=$((index+1))
    if [ "$done" -ge 1000 ]; then
      break
    fi
done
ls