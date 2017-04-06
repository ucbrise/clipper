import subprocess

if __name__ == "__main__":
    subprocess.call(
        'conda env create -f=/model/environment.yml -n predict-func-environment',
        shell=True)
    subprocess.call(
        '/bin/bash -c "source activate predict-func-environment" && exec python /container/predict_container.py',
        shell=True)
