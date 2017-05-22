FROM continuumio/anaconda:latest

MAINTAINER Dan Crankshaw <dscrankshaw@gmail.com>


RUN mkdir -p /model \
      && apt-get update \
      && apt-get install -y libzmq3 libzmq3-dev \
      && conda install -y pyzmq

WORKDIR /container

COPY containers/python/__init__.py containers/python/rpc.py /container/

ENV CLIPPER_MODEL_PATH=/model

# vim: set filetype=dockerfile:
