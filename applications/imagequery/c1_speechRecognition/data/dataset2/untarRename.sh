#!/bin/bash

cd /container/data/dataset2
tar -zxf flickr_audio.tar.gz
# tar -xzvf flickr_audio.tar.gz

cd /container/data/dataset2/flickr_audio/wavs
index=0;
for name in *.wav
do
    mv "${name}" "${index}.wav"
    index=$((index+1))
    if [ $index -ge 1000 ]; then
      break
    fi
done

echo "$(ls *.wav | wc -l) wav files in this dataset."