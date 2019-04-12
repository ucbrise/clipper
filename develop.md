#Windows Subsystem Linux

docker run -it --network=host -v /c/code:/code -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp zsxhku/clipper_test:version1

docker run -it --network clipper_network -e PROXY_NAME=proxytest -e PROXY_PORT=22223 proxytest


docker run -it --network clipper_network -e MODEL_NAME =grpctest -e MODEL_PORT=22222 grpctest

###Show all docker logs 
docker ps -q | xargs -L 1 docker logs
#MacOS


         