#!/bin/bash
# This shellscript is for downloading model meta
# Reference: https://stackoverflow.com/questions/48133080/how-to-download-a-google-drive-url-via-curl-or-wget/48133859
# https://drive.google.com/open?id=13jUMlN4wHgTzfr0CF9KuO0VYOC7jN-mX
fileid="13jUMlN4wHgTzfr0CF9KuO0VYOC7jN-mX"
filename="newmodel.ckpt-2000000.meta"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}