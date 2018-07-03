FROM python:3

WORKDIR /usr/src/app

RUN pip install kubernetes

COPY . .

COPY ~/.minikube /root/.minikube

CMD ["python", "list_pods.py"]
