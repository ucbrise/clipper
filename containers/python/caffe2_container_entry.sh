#!/usr/bin/env sh

IMPORT_ERROR_RETURN_CODE=3

echo "Attempting to run Caffe2 container without installing dependencies"
echo "Contents of /model"
ls /model/
/bin/bash -c "exec python /container/caffe2_container.py"
if [ $? -eq $IMPORT_ERROR_RETURN_CODE ]; then
	echo "Running Caffe2 container without installing dependencies fails"
	echo "Will install dependencies and try again"
  conda install -y --file /model/conda_dependencies.txt
  pip install -r /model/pip_dependencies.txt
  /bin/bash -c "exec python /container/caffe2_container.py"
fi
