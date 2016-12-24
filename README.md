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

For example:

```bash
$ cd $CLIPPER_ROOT_DIR
$ ./configure
$ cd debug
$ make

# write some code
$ make

# build and run unit tests with googletest
$ make unittests

# start the REST interface
$ ./frontends/rest
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
brew install cmake boost --c++11 zeromq hiredis libev

```

On Linux, depending on which distro and version you are running, the supplied packages for
some of these dependencies are too old. You can try installing from your distro's package
repository, and if the version of a dependency is too old you may have to build it from source.

