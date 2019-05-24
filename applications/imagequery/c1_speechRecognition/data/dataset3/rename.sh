#!/bin/bash

cd /container/data/dataset3/recordings

index=0;
for name in *.mp3
do
    mv "${name}" "${index}.wav"
    index=$((index+1))
    if [ $index -ge 1000 ]; then
      break
    fi
done

echo "$(ls *.wav | wc -l) wav files in this dataset."