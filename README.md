# Clipper Prediction Server

[![License](http://img.shields.io/:license-Apache%202-red.svg)](LICENSE)

## About the Project

![Clipper System Overview](images/arch_diagram.png)


Machine learning is being deployed in a growing number of applications which demand real-time, accurate, and robust predictions under heavy query load. However, most machine learning frameworks and systems only address model training and not deployment.

Clipper is the first general-purpose low-latency prediction serving system.  Interposing between end-user applications and a wide range of machine learning frameworks, Clipper introduces a modular architecture to simplify model deployment across frameworks.  Furthermore, by introducing caching, batching, and adaptive model selection techniques, Clipper reduces prediction latency and improves prediction throughput, accuracy, and robustness without modifying the underlying machine learning frameworks.


### Additional Resources

+ [Design Doc (work-in-progress)](https://docs.google.com/document/d/1Ghc-CAKXzzRshSa6FlonFa5ttmtHRAqFwMg7vhuJakw/edit?usp=sharing)
+ [Research Paper](https://arxiv.org/abs/1612.03079)

Clipper is a project in the UC Berkeley [RISE Lab](https://rise.cs.berkeley.edu/).

![RISE Lab logo](images/rise_lab_logo.png)



## Developing

Clipper is distributed through GitHub.

Clone the repository and submodules:
```
$ git clone --recursive https://github.com/ucbrise/clipper.git
```

### Dependencies

+ Boost >= 1.60
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

### Building

First generate the cmake files with `./configure`. This generates an out-of-source build directory called `debug`.
Go into one of this directory and then run `make` to actually
compile the code. You should only need to re-configure if you change one of the `CMakeLists.txt` files.
To build for release, run `./configure --release` which generates the `release` build directory instead of debug.
If you want to clean everything up, you can run `./configure --cleanup`.

__NOTE:__ Redis must be installed and on your path to run both the Query REST frontend and the unit-tests.

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

Clipper has been tested on OSX 10.11, 10.12, and on Debian stretch/sid and Ubuntu 12.04 and 16.04. It does not support Windows.

## Next steps

### Querying Clipper

For an example of querying Clipper, see the Python [example client](examples/example_client.py).

### Running Clipper in Docker

Clipper can also be run in Docker containers. See the [Docker guide](docker/README.md) for details.

## Contributing

To file a bug or request a feature, please file an issue. Pull requests are welcome.

Our mailing list is <clipper-dev@googlegroups.com>. For more information about the project, please contact Dan Crankshaw (crankshaw@cs.berkeley.edu).

Development planning and progress is tracked with the [Clipper Jira](https://clipper.atlassian.net/projects/CLIPPER/issues).
