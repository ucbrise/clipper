#!/usr/bin/env sh

conda env create -f=/model/environment.yml -n container-env
/bin/bash -c "source activate container-env && exec python /container/python_container.py"