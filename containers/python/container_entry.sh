#!/usr/bin/env sh

NAME=$1
CONTAINER_SCRIPT_PATH=$2

IMPORT_ERROR_RETURN_CODE=3

/bin/bash -c "exec python $CONTAINER_SCRIPT_PATH"

if [ $? -eq $IMPORT_ERROR_RETURN_CODE ]; then
  echo "Encountered an ImportError when running container. You can use the pkgs_to_install argument when calling clipper_admin.build_model() to supply any needed Python packages."
  exit 1
fi

echo "Encountered error not related to missing packages. Please refer to the container log to diagnose."
exit 1
