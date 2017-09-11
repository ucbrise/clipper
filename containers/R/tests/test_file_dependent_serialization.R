#!/usr/bin/env Rscript
require("Rclipper")

source("../deserialize_model.R")
source("test_helper.R")

time_format = "%Y-%m-%d-%H%-%M-%OS"
# Force seconds field in timestamp
# to print to millisecond resolution
op = options(digits.secs=3)

relative_file_path = format(Sys.time(), time_format)

# Reset options to their preexisting state
options(op)

dep_file_path = file.path("/tmp", relative_file_path)
dep_file = file(dep_file_path, open="w")

writeLines("some file text 123 @$%1 |*", dep_file)

close(dep_file)

func = function(x) {
  if(x > 0) {
    return(x)
  } else {
    text = readChar(dep_file_path, file.info(dep_file_path)$size)
    return(text)
  }
}

output_1 = func(10)
output_2 = func(-5)

output_path = create_output_directory()

Rclipper::serialize_function(as.character(substitute(func)), output_path)

dep_file = NULL
dep_file_path = NULL
func = NULL

deserialize_model(output_path)

stopifnot(func(10) == output_1)
stopifnot(func(-5) == output_2)

print("Success!")