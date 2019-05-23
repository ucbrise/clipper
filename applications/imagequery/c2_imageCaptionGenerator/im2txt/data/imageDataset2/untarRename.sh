#!/bin/bash

cd /container/im2txt/data/imageDataset2
tar -zxf flickr_audio.tar.gz
# tar -xzvf flickr_audio.tar.gz

cd /container/im2txt/data/imageDataset2/flickr_audio/wavs
index=0;
for name in *.wav
do
    mv "${name}" "${index}.wav"
    index=$((index+1))
    if [ $index -ge 1000 ]; then
      break
    fi
done