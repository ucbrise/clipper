FROM ubuntu:bionic
RUN apt-get update
RUN apt-get install python3 -y
RUN apt-get install python3-pip -y
RUN pip3 install grpcio


RUN pip3 install tensorflow
RUN pip3 install regex

ADD app /container/
CMD ["/container/container_entry.sh", "tensorflow-container", "/container/server.py"]


EXPOSE 22222