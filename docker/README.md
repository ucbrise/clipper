# Running Clipper with Docker

Clipper runs multiple separate processes. One simple way to orchestrate
all of these processes is to run each one in a Docker container and use
[`docker-compose`](https://docs.docker.com/compose/) to start and stop them
all simultaneously. The docker-compose script in this directory specifies the
dependencies between containers and the runtime arguments for each container.

From within this directory, run
```bash
$ docker-compose up query_frontend
```
to start the services. You can then interact with Clipper normally.
