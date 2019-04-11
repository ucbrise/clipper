#/bin/bash



docker build  --build-arg APPNAME=simpledag --build-arg CONTAINER=1 -f ./simpledag/simpledagDockerfile -t simpledag:container1 .
docker build  --build-arg APPNAME=simpledag --build-arg CONTAINER=2 -f ./simpledag/simpledagDockerfile -t simpledag:container2 .
docker build  --build-arg APPNAME=simpledag --build-arg CONTAINER=3 -f ./simpledag/simpledagDockerfile -t simpledag:container3 .
docker build  --build-arg APPNAME=simpledag --build-arg CONTAINER=4 -f ./simpledag/simpledagDockerfile -t simpledag:container4 .
docker build  --build-arg APPNAME=simpledag --build-arg CONTAINER=5 -f ./simpledag/simpledagDockerfile -t simpledag:container5 .

