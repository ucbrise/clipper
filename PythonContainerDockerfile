ARG CODE_VERSION=develop
FROM clipper/py-rpc:${CODE_VERSION}

MAINTAINER Dan Crankshaw <dscrankshaw@gmail.com>

RUN apt-get install -yqq build-essential libssl-dev libffi-dev python-dev

COPY clipper_admin/python_container_conda_deps.txt /lib/
RUN conda install -y --file /lib/python_container_conda_deps.txt

COPY containers/python/python_container.py containers/python/python_container_entry.sh /container/
COPY clipper_admin/ /lib/clipper_admin/
COPY VERSION.txt /lib/


CMD ["/container/python_container_entry.sh"]

# vim: set filetype=dockerfile:
