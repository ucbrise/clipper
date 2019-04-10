FROM ubuntu:bionic
RUN apt-get update
RUN apt-get install python3 -y
RUN apt-get install python3-pip -y
RUN pip3 install grpcio
RUN pip3 install protobuf

ADD simpleproxy/app /container/
#COPY app /container/

EXPOSE 22223


CMD ["/container/container_entry.sh", "tensorflow-container", "/container/server.py"]


