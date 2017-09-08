#' Builds a Docker image for a Clipper model container that can be launched to serve a model
#' with the provided prediction function
#' 
#' @param model_name string (character vector of length 1). The name to assign to the model image.
#' @param model_version string (character vector of length 1). The version tag to assign to the model image.
#' @param prediction_function function. This should accept a type-homogeneous list of 
#' inputs and return a list of outputs of the same length. If the elements of the output list
#' are not strings (character vectors of length 1), they will be converted to a serialized
#' string representation via 'jsonlite'.
#' @param sample_input For a prediction function that accepts a list of inputs of type X,
#' this should be a single input of type X. This is used to validate the compatability
#' of the function with Clipper and to determine the Clipper data type (bytes, ints, strings, etc)
#' to associate with the model.
#' @param model_registry string (character vector of length 1). The name of the image registry
#' to which to upload the model image. If NULL, the image will not be uploaded to a registry.
build_model = function(model_name, model_version, prediction_function, sample_input, model_registry=NULL) {
  sample_output <- tryCatch({
    prediction_function(list(sample_input))
  }, error = function(e) {
    err_msg = 
      sprintf("Encountered an error while evaluating 
              prediction function on sample input: %s", e)
    stop(err_msg)
  })
  if(class(sample_output) != "list") {
    err_msg = 
      sprintf("Prediction function must return a list, but
              evaluation on the provided sample input returned 
              an object of type: %s", class(sample_output))
  }
  
  base_model_path = "/tmp/r_models/"
  dir.create(file.path(base_model_path), showWarnings = FALSE)
  
  time_format = "%Y-%m-%d-%H%-%M-%OS"
  # Force seconds field in timestamp
  # to print to millisecond resolution
  op = options(digits.secs=3)
  
  relative_model_path = format(Sys.time(), time_format)
  
  # Reset options to their preexisting state
  options(op)
  
  full_model_path = file.path(base_model_path, relative_model_path)
  dir.create(full_model_path)
  
  prediction_function_name = as.character(substitute(prediction_function))
  .serialize_function(prediction_function_name, full_model_path)
  
  sample_input_path = file.path(base_model_path, relative_model_path, "sample.rds")
  saveRDS(sample_input, sample_input_path)
  
  package_path <- paste(system.file(package="rclipper"), "build_container.py", sep="/")
  
  if(missing(model_registry)) {
    python_call = sprintf("python %s -m %s -n %s -v %s",
                          package_path,
                          full_model_path, 
                          model_name,
                          model_version)
  } else {
    python_call = sprintf("python %s -m %s -n %s -v %d -r %s",
                          package_path,
                          full_model_path, 
                          model_name,
                          model_version,
                          model_registry)
  }
  
  system(python_call)
}