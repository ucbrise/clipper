#!/usr/bin/env Rscript
require("Rclipper")

source("../deserialize_model.R")
source("test_helper.R")

fib_or_return = function(x, y) {
  if(y == -1) {
    return(-x)
  } else {
    return(compute_fib(x))
  }
}

compute_fib = function(x) {
  result = fib_helper(x)
  return(fib_or_return(result,-1))
}

fib_helper = function(x) {
  if(x == 0 || x == 1) {
    return(x)
  }
  return(fib_helper(x - 1) + fib_helper(x - 2))
}

output_path = create_output_directory()

Rclipper::serialize_function(as.character(substitute(fib_or_return)), output_path)

original_fn = fib_or_return

fib_or_return = NULL
compute_fib = NULL
fib_helper = NULL

deserialize_model(output_path)

stopifnot(fib_or_return(6,0) == original_fn(6,0))
stopifnot(fib_or_return(10,-1) == original_fn(10,-1))
stopifnot(fib_or_return(2,5) == original_fn(2,5))

print("Success!")
