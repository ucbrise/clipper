FROM python:3

WORKDIR /usr/src/app

RUN pip install kubernetes

COPY . .

COPY ~/.kube /root/.kube
COPY ~/.minikube /root/.minikube

CMD ["python", "list_pods.py"]
