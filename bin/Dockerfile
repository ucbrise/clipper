FROM python:3

WORKDIR /usr/src/app

RUN pip install kubernetes

COPY . .

COPY /home/jenkins/.minikube /root/.minikube
COPY /home/jenkins/.kube /root/.kube

CMD ["python", "list_pods.py"]
