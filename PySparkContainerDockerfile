ARG CODE_VERSION=develop
FROM clipper/py-rpc:${CODE_VERSION}

COPY clipper_admin/clipper_admin/python_container_conda_deps.txt /lib/

RUN echo deb http://ftp.de.debian.org/debian jessie-backports main >> /etc/apt/sources.list \
      && apt-get update --fix-missing \
      && apt-get install -yqq -t jessie-backports openjdk-8-jdk \
      && conda install -y --file /lib/python_container_conda_deps.txt \
      && pip install pyspark

COPY containers/python/pyspark_container.py containers/python/pyspark_container_entry.sh /container/
COPY VERSION.txt /lib/
COPY clipper_admin/ /lib/clipper_admin
RUN pip install /lib/clipper_admin

CMD ["/container/pyspark_container_entry.sh"]

# vim: set filetype=dockerfile:
