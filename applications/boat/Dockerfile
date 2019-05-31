# docker run -it --name -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp boat_image
FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install python3 python3-pip -y

COPY boat/requirements.txt /container
RUN pip3 install -r requirements.txt

RUN pip3 install git+https://github.com/ucbrise/clipper.git@develop#subdirectory=clipper_admin


RUN ls /container
RUN pwd