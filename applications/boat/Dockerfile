FROM zsxhku/clipperpy35dev
RUN apt-get update
RUN apt-get install python3 python3-pip -y
RUN apt-get install git-core -y
COPY boat/requirements.txt /container/requirements.txt
RUN pip3 install -r /container/requirements.txt


WORKDIR /container
RUN git clone https://github.com/jitaogithub/boat.git
WORKDIR /container/boat
RUN pip3 install git+https://github.com/ucbrise/clipper.git@develop#subdirectory=clipper_admin
RUN mv $(find / -type d -name clipper_admin) /container/boat
CMD python3 deploy.py

EXPOSE 10000
