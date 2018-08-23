mkdir pyt_docker

cd pyt_docker

curl -o Dockerfile https://raw.githubusercontent.com/janetsungczi/clipper/pyt_0.4.1_fix/dockerfiles/CustomPyTorchContainerDockerfile

curl -o pytorch_container.py https://raw.githubusercontent.com/janetsungczi/clipper/pyt_0.4.1_fix/containers/python/pytorch_container.py

docker build -t custom-pytorch36-container:1 .