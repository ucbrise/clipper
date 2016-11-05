# Clipper Prediction Server


[Design doc (WIP)](https://docs.google.com/a/berkeley.edu/document/d/1Ghc-CAKXzzRshSa6FlonFa5ttmtHRAqFwMg7vhuJakw/edit?usp=sharing)

## Build Instructions

First generate the cmake files with `./configure`. This generates two out-of-source build directories, `release` and `debug`.
Go into one of these directories (you probably want to be in `debug` for day-to-day development) and then run `make` to actually
compile the code. You should only need to re-configure if you change one of the `CMakeLists.txt` files.
If you want to clean everything up, you can run `./configure --cleanup`.

For example:

```bash
$ cd $CLIPPER_ROOT_DIR
$ ./configure
$ cd debug
$ make

# write some code
$ make

# run the bench binary
$ cd frontends
$ ./bench
```

### Dependencies

+ Boost >= 1.62
+ cmake >= 3.2

On a Mac you can install these with `brew install cmake boost`.
