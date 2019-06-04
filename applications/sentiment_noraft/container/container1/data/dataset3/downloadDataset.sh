#!/bin/bash
fileid="1H74h6bDqiOzQNRX5C3ttuVIEIXlMteLl"
filename="speech-accent-archive.zip"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}



