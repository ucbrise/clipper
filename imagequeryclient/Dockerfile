FROM python:3

RUN pip install --no-cache-dir grpcio
RUN pip install --no-cache-dir protobuf


ADD app /app
#COPY app /container/
#CMD [ "python", "/app/imagequery.py" ]
ENTRYPOINT [ "python", "/app/imagequery.py" ]


