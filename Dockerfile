FROM ubuntu:16.04

# Install common tools: make, git, cmake, gcc, g++, libev-dev, redis-server, wget, unzip, etc.
RUN sed 's/main$/main universe/' -i /etc/apt/sources.list
RUN apt-get update
RUN apt-get install -y make
RUN apt-get install -y git
RUN apt-get install -y software-properties-common
RUN add-apt-repository ppa:george-edison55/cmake-3.x
RUN apt-get update
RUN apt-get install -y cmake
RUN apt-get upgrade -y
RUN apt-get install -y gcc
RUN apt-get install -y g++
RUN apt-get install -y libev-dev
RUN add-apt-repository ppa:chris-lea/redis-server
RUN apt-get update
RUN apt-get install -y redis-server
RUN apt-get install -y wget
RUN apt-get install -y unzip

# Install Hiredis.
RUN git clone https://github.com/redis/hiredis.git
RUN cd hiredis/ && make && make install

# Install Boost 1.63.
RUN wget -O boost_1_63_0.zip https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip/download
RUN unzip boost_1_63_0.zip -d boost
RUN cd boost/ && ./bootstrap.sh && ./b2 && ./bjam install

# Install ZeroMQ 4.1.6.
RUN mkdir -p /usr/local/clipper
ADD . /usr/local/clipper
WORKDIR "/usr/local/clipper"
RUN wget https://github.com/zeromq/zeromq4-1/releases/download/v4.1.6/zeromq-4.1.6.zip
RUN unzip zeromq-4.1.6.zip -d zeromq
RUN cd zeromq/zeromq-4.1.6 && ./configure && make && make install

# Build Clipper.
RUN ./configure && cd debug && make
RUN ./run_unittests.sh
RUN ./start_clipper.sh
