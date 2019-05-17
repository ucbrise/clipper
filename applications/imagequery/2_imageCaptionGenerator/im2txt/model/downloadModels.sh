#!/bin/bash
# This shellscript is for downloading model meta, newmodel.ckpt-2000000.meta, and model file, newmodel.ckpt-2000000.data-00000-of-00001.
# Reference: https://stackoverflow.com/questions/48133080/how-to-download-a-google-drive-url-via-curl-or-wget/48133859

# Download model meta
# https://drive.google.com/open?id=13jUMlN4wHgTzfr0CF9KuO0VYOC7jN-mX
fileid="13jUMlN4wHgTzfr0CF9KuO0VYOC7jN-mX"
filename="newmodel.ckpt-2000000.meta"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}


# Download model file
# https://drive.google.com/open?id=1nWpj5_DlA8Ky43vncMZrB9FXmGGu6zMq
fileid="1nWpj5_DlA8Ky43vncMZrB9FXmGGu6zMq"
filename="newmodel.ckpt-2000000.data-00000-of-00001"
curl -c ./cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb ./cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' ./cookie`&id=${fileid}" -o ${filename}

