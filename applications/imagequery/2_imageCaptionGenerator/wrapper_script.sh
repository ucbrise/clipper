#!/bin/bash

# This file might need to be edited with notepad++, take a look at the references below
# reference: https://github.com/postlight/headless-wp-starter/issues/171
# reference: http://sql313.com/index.php/43-main-blogs/maincat-dba/62-using-notepad-to-change-end-of-line-characters

# I. Installing bazel
# 1. install required packages for installation
apt-get install pkg-config zip zlib1g-dev unzip
# 2. set access level to the installer
chmod +x /container/bazel-0.25.1-installer-linux-x86_64.sh
# 3. run bazel installer
/container/bazel-0.25.1-installer-linux-x86_64.sh --user
# 4. add to system path
export PATH="$PATH:$HOME/bin"
# Finish installing bazel 

# II. Set access level to the shellscript to be executed with bazel
chmod 777 /container/workspace/im2txt/runWithArg.sh

# III. Download model from google drive
# Reference: https://stackoverflow.com/questions/48133080/how-to-download-a-google-drive-url-via-curl-or-wget/48133859
/container/workspace/im2txt/model/newDownload1.sh # /notebooks/newmodel.ckpt-2000000.meta 
/container/workspace/im2txt/model/newDownload2.sh # /notebooks/newmodel.ckpt-2000000.data-00000-of-00001
mv /notebooks/newmodel.ckpt-2000000.meta /container/workspace/im2txt/model
mv /notebooks/newmodel.ckpt-2000000.data-00000-of-00001 /container/workspace/im2txt/model

echo "finished wrapper script"
# run simlerpc server
python /container/predict.py