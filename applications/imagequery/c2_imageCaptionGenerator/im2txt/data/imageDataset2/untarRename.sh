#!/bin/bash

cd /container/im2txt/data/imageDataset2
tar -zxvf flickr_audio.tar.gz

cd /container/im2txt/data/imageDataset2/flickr_audio
index=0;
for name in *.wav
do
    mv "${name}" "${index}.wav"
    index=$((index+1))
done
ls