# Clipper Prediction Server


[Design doc (WIP)](https://docs.google.com/a/berkeley.edu/document/d/1Ghc-CAKXzzRshSa6FlonFa5ttmtHRAqFwMg7vhuJakw/edit?usp=sharing)

## Build Instructions

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

On a Mac you can install these with 
```
brew install cmake boost --c++11 zeromq
```

## For Developers

Before submitting a pull request, run use Clang-Format to format it with the
Google style guide. Many editors have clang-format integrations so that it
will run automatically when you save a file, or you can run it from the command
line with `clang-format -style=Google -i my_edited_header.hpp my_edited_file.cpp
my_other_edited_file.cpp`. Alternatively, to run it on the entire source directory:
```
$ find src/ -name '*.cpp' -o -name '*.hpp' | xargs clang-format -style=Google -i
```

