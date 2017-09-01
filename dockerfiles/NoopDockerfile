ARG CODE_VERSION=develop
FROM clipper/py-rpc:${CODE_VERSION}

MAINTAINER Dan Crankshaw <dscrankshaw@gmail.com>

COPY containers/python/noop_container.py /container/

CMD ["python", "/container/noop_container.py"]

# vim: set filetype=dockerfile:
