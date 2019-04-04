FROM linuxbrew/linuxbrew:latest

RUN sudo apt-get update
RUN sudo apt-get install python3 -y
RUN sudo apt-get install python3-pip -y
RUN sudo pip3 install grpcio



RUN brew tap watsonbox/cmu-sphinx
RUN brew install --HEAD watsonbox/cmu-sphinx/cmu-sphinxbase


RUN brew install bison
RUN brew install --HEAD watsonbox/cmu-sphinx/cmu-pocketsphinx



ADD app /app/

EXPOSE 22222