#!/usr/bin/env Rscript
library("Rclipper")
library("randomForest")

source("../deserialize_model.R")
source("test_helper.R")

model <- randomForest(mpg~., mtcars, keep.forest=TRUE)

pred_fn = function(inputs) {
  outputs = list()
  for(i in seq_along(inputs)) {
    outputs = c(outputs, predict(model, inputs[i,]))
  }
  return(outputs)
}

outputs = pred_fn(mtcars)

output_path = create_output_directory()
Rclipper::serialize_function(as.character(substitute(pred_fn)), output_path)

model = NULL
pred_fn = NULL

deserialize_model(output_path)

new_outputs = pred_fn(mtcars)

stopifnot(length(new_outputs) == length(outputs))
for(i in seq_along(new_outputs)) {
  stopifnot(new_outputs[[i]] == outputs[[i]])
}

print("Success!")
