#!/bin/bash
fileid="1bNaOBthTNME64qkeKqNRYdMMogmZZJpc"
filename="mask_rcnn_coco.h5"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


