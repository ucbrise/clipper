#!/bin/bash
fileid="1srlfuqyX9zfZCxgnzSjWibZKv-U56WuC"
filename="shape_predictor_68_face_landmarks.dat"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


