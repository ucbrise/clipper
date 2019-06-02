#!/bin/bash
fileid="1ZHw9yOA7ClZRSUqejmaIGZOh5WxOeYzx"
filename="part1.tar.gz"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


##
