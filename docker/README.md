# Running Clipper with Docker

Clipper runs multiple separate processes. One simple way to orchestrate
all of these processes is to run each one in a Docker container and use
[`docker-compose`](https://docs.docker.com/compose/) to start and stop them
all simultaneously. The docker-compose script in this directory specifies the
dependencies between containers and the runtime arguments for each container.

## Installation:

Consult the [Docker documenation](https://docs.docker.com/engine/installation/)
for instructions on installing the Docker engine.

Once Docker is installed, you can install docker-compose using your
OS's package manager.

On Debian/Ubuntu:
```bash
$ apt-get install docker-compose
```

On a Mac:
```bash
$ brew install docker-compose
```

## Running Clipper with Docker-Compose

From within this directory (`$CLIPPER_ROOT/docker/`), run
```bash
$ docker-compose up -d query_frontend
```
to start the services. You can then interact with Clipper normally.

> Note: You have to run the `docker-compose` commands from within this
> directory because the command looks for a `docker-compose.yml` file in the
> current directory to resolve container names. Consult the Docker-Compose
> [documentation](https://docs.docker.com/compose/gettingstarted/#/step-4-build-and-run-your-app-with-compose)
> for more information.

```bash
# See all running docker containers
$ docker ps
# or just the containers docker-compose started
$ docker-compose ps

# view logs for container named docker_query_frontend_1
$ docker logs docker_query_frontend_1

# tail a logfile
$ docker logs --follow docker_query_frontend_1
```

You can run the [client example](../examples/example_client.py) to create
a new application endpoint and start querying Clipper.

## Stopping Clipper with Docker-Compose

To stop the Clipper containers, from within this directory run:
```bash
$ docker-compose stop
```

