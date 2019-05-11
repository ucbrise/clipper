FROM python:3

RUN pip install --no-cache-dir grpcio
RUN pip install --no-cache-dir protobuf

ADD app /app
#COPY app /container/
#CMD [ "python", "/app/grpc_client.py" ]
ENTRYPOINT [ "python", "/app/auto_pilot_client.py" ]


