#/bin/bash

docker build -f ./translation/Dockerfile1 -t translation:container1 .
docker build -f ./translation/Dockerfile2 -t translation:container2 .
docker build -f ./translation/Dockerfile3 -t translation:container3 .
docker build -f ./translation/Dockerfile4 -t translation:container4 .

