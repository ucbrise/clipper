#!/bin/bash
fileid="1gL5eTyQ7nhqeLydUznlL9ahr9wZPT47R"
filename="wordsList.npy"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}



