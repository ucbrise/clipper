FROM ubuntu:bionic
RUN apt-get update
RUN apt-get install python3 -y
RUN apt-get install python3-pip -y
RUN pip3 install grpcio


RUN apt-get upgrade -y
RUN apt-get install -y build-essential make cmake scons curl git \
                               ruby autoconf automake autoconf-archive \
                               gettext libtool flex bison \
                               libbz2-dev libcurl4-openssl-dev \
                               libexpat-dev libncurses-dev

RUN git clone https://github.com/Homebrew/linuxbrew.git ~/.linuxbrew

RUN export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib64/pkgconfig:/usr/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib64/pkgconfig:/usr/share/pkgconfig:$PKG_CONFIG_PATH
## Setup linux brew
RUN export LINUXBREWHOME=$HOME/.linuxbrew
RUN export PATH=$LINUXBREWHOME/bin:$PATH
RUN export MANPATH=$LINUXBREWHOME/man:$MANPATH
RUN export PKG_CONFIG_PATH=$LINUXBREWHOME/lib64/pkgconfig:$LINUXBREWHOME/lib/pkgconfig:$PKG_CONFIG_PATH
RUN export LD_LIBRARY_PATH=$LINUXBREWHOME/lib64:$LINUXBREWHOME/lib:$LD_LIBRARY_PATH


RUN brew tap watsonbox/cmu-sphinx
RUN brew install --HEAD watsonbox/cmu-sphinx/cmu-sphinxbase
RUN brew install --HEAD watsonbox/cmu-sphinx/cmu-pocketsphinx


ADD app /app/

EXPOSE 22222