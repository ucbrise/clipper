ARG CODE_VERSION=develop
FROM clipper/py-rpc:${CODE_VERSION}

MAINTAINER Dan Crankshaw <dscrankshaw@gmail.com>

COPY containers/python/sklearn_cifar_container.py /container/

CMD ["python", "/container/sklearn_cifar_container.py"]

# vim: set filetype=dockerfile:
