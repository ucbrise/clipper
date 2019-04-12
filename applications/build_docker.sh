#/bin/bash



docker build -f ./simpledag/simpledagDockerfile --build-arg APP=simpledag --build-arg CONTAINER=1  -t simpledag:container1 .
docker build -f ./simpledag/simpledagDockerfile --build-arg APP=simpledag --build-arg CONTAINER=2  -t simpledag:container2 .
docker build -f ./simpledag/simpledagDockerfile --build-arg APP=simpledag --build-arg CONTAINER=3 -t simpledag:container3 .
docker build -f ./simpledag/simpledagDockerfile --build-arg APP=simpledag --build-arg CONTAINER=4 -t simpledag:container4 .
docker build -f ./simpledag/simpledagDockerfile --build-arg APP=simpledag --build-arg CONTAINER=5 -t simpledag:container5 .

