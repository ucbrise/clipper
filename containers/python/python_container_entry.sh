#!/usr/bin/env sh

IMPORT_ERROR_RETURN_CODE=3

CONDA_DEPS_PATH="/model/conda_dependencies.txt"
PIP_DEPS_PATH="/model/pip_dependencies.txt"
CONTAINER_SCRIPT_PATH="/container/python_container.py"

test -f $CONDA_DEPS_PATH
dependency_check_returncode=$?

if [ $dependency_check_returncode -eq 0 ]; then
	echo "First attempting to run Python container without installing supplied dependencies."
else
	echo "No dependencies supplied. Attempting to run Python container."
fi

/bin/bash -c "exec python $CONTAINER_SCRIPT_PATH"

if [ $? -eq $IMPORT_ERROR_RETURN_CODE ]; then
	if [ $dependency_check_returncode -eq 0 ]; then
		echo "Encountered an ImportError when running Python container without installing supplied dependencies."
		echo "Will install supplied dependencies and try again."
	  conda install -y --file $CONDA_DEPS_PATH
	  pip install -r $PIP_DEPS_PATH

	  /bin/bash -c "exec python $CONTAINER_SCRIPT_PATH"

	  if [ $? -eq $IMPORT_ERROR_RETURN_CODE ]; then
	  	echo "Encountered an ImportError even after installing supplied dependencies."
	  	exit 1
	  fi
	else
		echo "Encountered an ImportError when running Python container."
		echo "Please supply necessary dependencies through an Anaconda environment and try again."
		exit 1
	fi
fi

echo "Encountered error not related to missing packages. Please refer to the container log to diagnose."
exit 1
