#!/usr/bin/env sh

conda env create -f=/model/environment.yml -n container-env
source activate container-env
python /container/python_container.py
