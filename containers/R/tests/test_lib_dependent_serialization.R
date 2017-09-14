#!/usr/bin/env Rscript
require("Rclipper")

source("../deserialize_model.R")
source("test_helper.R")

require("jsonlite")

vec = c(1,2,3,4)

func = function(y) {
  x = 10
  return(toJSON(x + y + vec))
}

output_path = create_output_directory()

Rclipper::serialize_function(as.character(substitute(func)), output_path)

output_1 = func(1)
output_2 = func(10)
output_3 = func(-74)

# Unloads the jsonlite package from scope.
# This package should be reloaded during
# the deserialization process, allowing for
# the error-free execution of the `toJSON` method
detach("package:jsonlite", unload=TRUE)

vec = NULL
func = NULL

deserialize_model(output_path)

stopifnot(func(1) == output_1)
stopifnot(func(10) == output_2)
stopifnot(func(-74) == output_3)

print("Success!")