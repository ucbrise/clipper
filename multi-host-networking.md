# Mulitiple host networking 


## Step 1 Start a KV store:
Configure Key/Value Store 

```sh
docker run -d --name consul \
-p 8300:8300 -p 8400:8400 -p 8500:8500 -p 53:8600/udp \
gliderlabs/consul-server:latest -bootstrap
```

## Step 2 Reconfigure docke daemons on each host:

Configure Docker Daemon on each host

### Option 1: Configure existing daemon

```sh
sudo systemctl stop docker
```

```sh
sudo dockerd --cluster-store=consul://[[CONSUL_IP]]:8500 --cluster-advertise=[[HOST_IP1]]:0
```

Below are examples:
```sh
sudo dockerd --cluster-store=consul://202.45.128.162:8500 --cluster-advertise=202.45.128.161:0 &

sudo dockerd -H tcp://0.0.0.0:2375 -H unix:///var/run/docker.sock --cluster-store=consul://202.45.128.162:8500 --cluster-advertise=202.45.128.173:0 &

sudo dockerd -H tcp://0.0.0.0:2375 -H unix:///var/run/docker.sock --cluster-store=consul://202.45.128.162:8500 --cluster-advertise=202.45.128.174:0 &

sudo dockerd -H tcp://0.0.0.0:2375 -H unix:///var/run/docker.sock --cluster-store=consul://202.45.128.162:8500 --cluster-advertise=202.45.128.175:0 &

```

### Option 2: start a new daemon

```sh
docker run --privileged --name d1 -d \
--net=host katacoda/dind:1.10 -H 0.0.0.0:3375 \
--cluster-store=consul://[[CONSUL_IP]]:8500 \
--cluster-advertise=[[HOST_IP]]:0
```

Export new host daemon
```sh
export DOCKER_HOST="[[HOST_IP1]]:3375"
```

## Step 3  Create an overlay network:
One one of the Docker hosts, create the network

```sh
docker network create -d overlay multihost-net
```

When new containers launch on any host they register themselves to the network

## Step 4:

Host 1

```sh
docker run -d --name ws1 --net=multihos-net katacoda/docker-http-server
```

Host 2
```sh
docker run --net=multihost-net benhall/curl curl -Ss ws1
```