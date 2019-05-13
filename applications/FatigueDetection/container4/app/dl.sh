#!/bin/bash
fileid="1uT6uv3a_J04O7UQfeGXKshJu5INRxigk"
filename="pose_iter_440000.caffemodel"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


