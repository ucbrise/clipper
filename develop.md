#Windows Subsystem Linux

docker run -it --network=host -v /c/code:/code -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp zsxhku/clipper_test:version1

docker run -it -e PROXY_NAME=test -e PROXY_PORT=22223 proxytest
#MacOS


         