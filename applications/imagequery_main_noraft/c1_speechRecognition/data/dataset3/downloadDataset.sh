#!/bin/bash
# This shellscript is for downloading speech-accent-archive.zip from google drive
# Reference: https://stackoverflow.com/questions/48133080/how-to-download-a-google-drive-url-via-curl-or-wget/48133859

# Download dataset
# https://drive.google.com/file/d/1H74h6bDqiOzQNRX5C3ttuVIEIXlMteLl/view?usp=sharing
fileid="1H74h6bDqiOzQNRX5C3ttuVIEIXlMteLl"
filename="speech-accent-archive.zip"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


