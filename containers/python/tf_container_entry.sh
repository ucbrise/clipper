#!/usr/bin/env sh

IMPORT_ERROR_RETURN_CODE=3

echo "Attempting to run TensorFlow container without installing any dependencies"
echo "Contents of /model"
ls /model/

/bin/bash -c "exec python /container/tf_container.py"
if [ $? -eq $IMPORT_ERROR_RETURN_CODE ]; then
	echo "Running TensorFlow container without installing dependencies fails"
	echo "Will install dependencies and try again"
  conda install -y --file /model/conda_dependencies.txt
  pip install -r /model/pip_dependencies.txt
  /bin/bash -c "exec python /container/tf_container.py"
fi