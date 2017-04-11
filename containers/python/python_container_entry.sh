#!/usr/bin/env sh

conda env create -f=/model/environment.yml -n container-env
source activate container-env
python -u /container/python_container.py
