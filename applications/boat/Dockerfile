# docker run -it --name -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp boat_image
FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install python3 python3-pip -y
RUN apt-get install git-core -y
RUN git config --global user.email "yy0125@connect.hku.hk"

COPY boat/requirements.txt /container/requirements.txt
RUN pip3 install -r /container/requirements.txt

RUN pip3 install git+https://github.com/ucbrise/clipper.git@develop#subdirectory=clipper_admin


RUN ls /container
RUN pwd
