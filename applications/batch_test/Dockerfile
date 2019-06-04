FROM ubuntu:bionic
RUN apt-get update; apt-get install python3 python3-all-dev python3-pip build-essential -y
RUN pip3 install grpcio protobuf


ADD batch_test/app /container/

ADD grpc/app/container_entry.sh /container/
ADD grpc/app/server.py /container/
ADD grpc/app/model_pb2_grpc.py /container/
ADD grpc/app/model_pb2.py /container/
ADD grpc/app/proxy_pb2_grpc.py /container/
ADD grpc/app/proxy_pb2.py /container/

EXPOSE 22222

CMD ["/container/container_entry.sh", "tensorflow-container", "/container/server.py"]
