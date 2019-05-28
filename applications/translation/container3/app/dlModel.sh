#!/bin/bash
fileid="1YY1wrUsKrg_0VRjJNbKxmOG7aZA4iMaS"
filename="models.zip"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}

