#!/usr/bin/env sh

IMPORT_ERROR_RETURN_CODE=3

CONDA_DEPS_PATH="/model/conda_dependencies.txt"
PIP_DEPS_PATH="/model/pip_dependencies.txt"
CONTAINER_SCRIPT_PATH="/container/python_closure_container.py"

LOCAL_PACKAGE_IMPORT_MSG="The ImportError may have been caused by a missing local package. A local package is one not found in the pip or conda distributions. Local package-based imports for modules of the form a.b.c need to exist in a local folder structure .../a/b/c.py in order to be supplid to the container."

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
	  	echo $LOCAL_PACKAGE_IMPORT_MSG
	  	exit 1
	  fi
	else
		echo "Encountered an ImportError when running Python container."
		echo $LOCAL_PACKAGE_IMPORT_MSG
		echo "If the ImportError was not caused by a missing local package, please supply necessary dependencies through an Anaconda environment and try again."
		exit 1
	fi
fi

echo "Encountered error not related to missing packages. Please refer to the container log to diagnose."
exit 1
