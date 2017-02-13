trap "exit" INT TERM
trap "kill 0" EXIT

python bench_init.py

export CLIPPER_MODEL_NAME="bench_sklearn_cifar"
export CLIPPER_MODEL_VERSION="1"
export CLIPPER_MODEL_PATH="data/"

python ../containers/python/sklearn_cifar_container.py 
