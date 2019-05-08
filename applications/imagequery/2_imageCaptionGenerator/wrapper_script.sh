#!/bin/bash

# This file might need to be edited with notepad++, take a look at the references below
# reference: https://github.com/postlight/headless-wp-starter/issues/171
# reference: http://sql313.com/index.php/43-main-blogs/maincat-dba/62-using-notepad-to-change-end-of-line-characters

# Installing bazel
# 1. install required packages for installation
apt-get install pkg-config zip zlib1g-dev unzip
# 2. set access level to the installer
chmod +x /container/bazel-0.25.1-installer-linux-x86_64.sh
# 3. run bazel installer
./container/bazel-0.25.1-installer-linux-x86_64.sh --user
# 4. add to system path
export PATH="$PATH:$HOME/bin"
# Finish installing bazel 

# set access level to the shellscript to be executed with bazel
chmod 777 /container/workspace/im2txt/runWithArg.sh

echo "finished wrapper script"
# run simlerpc server
python3 /container/server.py 2 9000