FROM clipper/py-rpc:latest

MAINTAINER Dan Crankshaw <dscrankshaw@gmail.com>

COPY python_container.py python_container_entry.sh /container/
COPY pywrencloudpickle.py python_container_conda_deps.txt /lib/

RUN conda install -y --file /lib/python_container_conda_deps.txt

CMD ["/container/python_container_entry.sh"]

# vim: set filetype=dockerfile:
