# Clipper Prediction Server

[Design doc (WIP)](https://docs.google.com/a/berkeley.edu/document/d/1Ghc-CAKXzzRshSa6FlonFa5ttmtHRAqFwMg7vhuJakw/edit?usp=sharing)

## Getting Clipper

Currently, Clipper must be built from source. First clone the repository and submodules:
```
git clone --recursive
```

## Build Instructions

Clipper depends on the [Redox](https://github.com/dcrankshaw/redox) git submodule to build.

If you've already cloned the Clipper repo without the submodule, you can run `git submodule update --init --recursive` to download
Redox. 

First generate the cmake files with `./configure`. This generates an out-of-source build directory called `debug`.
Go into one of this directory and then run `make` to actually
compile the code. You should only need to re-configure if you change one of the `CMakeLists.txt` files.
To build for release, run `./configure --release` which generates the `release` build directory instead of debug.
If you want to clean everything up, you can run `./configure --cleanup`.

__NOTE:__ Redis must be installed and on your path to run both the query REST frontend and the unit-tests.

For example:

```bash
$ cd $CLIPPER_ROOT_DIR
$ ./configure
$ cd debug
$ make

# write some code and compile it
$ make

# build and run unit tests with googletest
$ ../bin/run_unittests.sh

# build and then start the query REST frontend
$ ../bin/start_clipper.sh
```

To query the REST API, send HTTP POST requests:
```bash
# Run this in a separate window
# Send JSON request using curl
$ curl -X POST --header "Content-Type:text/json" -d '{"uid": 1, "input": [1, 2, 3, 4]}' 127.0.0.1:1337/predict
```

### Dependencies

+ Boost >= 1.62
+ cmake >= 3.2
+ zeromq >= 4.1.6
+ hiredis
+ libev
+ redis-server >= 3.2

On a Mac you can install these with 
```
brew install cmake boost --c++11 zeromq hiredis libev redis

```
On Debian stretch/sid:
```
sudo apt-get install cmake libzmq5 libzmq5-dev libhiredis-dev libev-dev libboost-all-dev
```

On other Linux distributions, depending on which distro and version you are running, the supplied packages for
some of these dependencies may be too old. You can try installing from your distro's package
repository, and if the version of a dependency is too old you may have to build it from source.

## Docker Support

We maintain Docker images for both the Query Processor and Management processes
on Docker Hub in the [Clipper repository](https://hub.docker.com/u/clipper/).

The recommended way of running Clipper in Docker is using docker-compose.
Check out the [guide](docker/README.md) for instructions.


## Contributing

We welcome bug reports and feature requests.

[Clipper Jira](https://clipper.atlassian.net/browse/CLIPPER)
