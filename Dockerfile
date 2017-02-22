FROM ubuntu:16.04

# Install common tools.
RUN apt-get update && \
	apt-get install -y --no-install-recommends make git software-properties-common && \
	add-apt-repository ppa:george-edison55/cmake-3.x && \
	add-apt-repository ppa:chris-lea/redis-server && \
	apt-get update && \
	apt-get install -y --no-install-recommends cmake gcc g++ libev-dev redis-server wget unzip

# Create directory for Clipper.
RUN mkdir -p /usr/local/clipper
ADD . /usr/local/clipper
WORKDIR "/usr/local/clipper"

# Install Hiredis.
RUN git clone https://github.com/redis/hiredis.git && \
	cd hiredis/ && make && make install && \
	cd ../ && rm -rf hiredis/

# Install Boost 1.63.
RUN wget -O boost_1_63_0.zip https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip/download && \
	unzip boost_1_63_0.zip -d boost && \
	cd boost/boost_1_63_0/ && ./bootstrap.sh && ./b2 && ./bjam install && \
	cd ../../ && rm -rf boost*

# Install ZeroMQ 4.1.6.
RUN wget https://github.com/zeromq/zeromq4-1/releases/download/v4.1.6/zeromq-4.1.6.zip && \
	unzip zeromq-4.1.6.zip -d zeromq && \
	cd zeromq/zeromq-4.1.6 && ./configure && make && make install && \
	cd ../../ && rm -rf zeromq*

# Build Clipper.
RUN ./configure && cd debug && make && cd ../ && \
	./configure --release && cd release && make -j2 query_frontend management_frontend
