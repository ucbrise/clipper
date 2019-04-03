FROM ubuntu:bionic
RUN apt-get update
RUN apt-get install python3 -y
RUN apt-get install python3-pip -y
RUN pip3 install grpcio
RUN pip3 install protobuf


#ADD app /container/
COPY simpledag/container1/app /container/
ADD grpc/app/container_entry.sh /container/
ADD grpc/app/server.py /container/
ADD grpc/app/test_pb2_grpc.py /container/
ADD grpc/app/test_pb2.py /container/

CMD ["/container/container_entry.sh", "tensorflow-container", "/container/server.py"]

EXPOSE 22222