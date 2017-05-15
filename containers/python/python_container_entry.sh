#!/usr/bin/env sh

conda install -y --file /model/conda_dependencies.txt
pip install -r /model/pip_dependencies.txt
/bin/bash -c "exec python /container/python_container.py"
