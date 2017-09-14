#!/usr/bin/env Rscript
require("Rclipper")

source("../deserialize_model.R")
source("test_helper.R")

identity_mat = diag(2)

B = matrix(
  c(1,2,3,4,5,6),
  nrow=2,
  ncol=3
)

matmul = function(A) {
  return(identity_mat %*% B %*% A)
}

A1 = matrix(
  seq(1,6),
  nrow=3,
  ncol=2
)

A2 = matrix(
  seq(7,12),
  nrow=3,
  ncol=2
)

A3 = matrix(
  seq(13,18),
  nrow=3,
  ncol=2
)

output_1 = matmul(A1)
output_2 = matmul(A2)
output_3 = matmul(A3)

output_path = create_output_directory()

Rclipper::serialize_function(as.character(substitute(matmul)), output_path)

identity_mat = NULL
B = NULL
matmul = NULL

deserialize_model(output_path)

stopifnot(matmul(A1) == output_1)
stopifnot(matmul(A2) == output_2)
stopifnot(matmul(A3) == output_3)

print("Success!")