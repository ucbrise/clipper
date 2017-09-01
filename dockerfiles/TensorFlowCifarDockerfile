ARG CODE_VERSION=develop
FROM clipper/py-rpc:${CODE_VERSION}

MAINTAINER Dan Crankshaw <dscrankshaw@gmail.com>

RUN conda install tensorflow

COPY containers/python/tf_cifar_container.py /container/

CMD ["python", "/container/tf_cifar_container.py"]

# vim: set filetype=dockerfile:
