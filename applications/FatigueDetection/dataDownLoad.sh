#!/bin/bash
fileid="1l6OPifJ0s3jFnEbXKZGf_cwm-nMtvOlT"
filename="ImageSet"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


