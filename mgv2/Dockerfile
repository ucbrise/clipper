FROM continuumio/miniconda

COPY ./mgv2/ /mgv2


RUN apt-get install -y python-dev
RUN apt-get install -y gcc
COPY ./clipper_admin/ /clipper_admin
RUN pip install /clipper_admin

RUN pip install requests\
&& pip install -r /mgv2/requirements.txt

ENV HOME /home

COPY ./mgv2/config /home/.kube/

ENTRYPOINT ["python", "/mgv2/mgv2.py"]
