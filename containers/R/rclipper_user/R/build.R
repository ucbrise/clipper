#' Builds a Docker image for a Clipper model container that can be launched to serve a model
#' with the provided prediction function
#' 
#' @param model_name character vector of length 1. The name to assign to the model image.
#' @param model_version character vector of length 1. The version tag to assign to the model image.
#' @param prediction_function function. This should accept a type-homogeneous list of 
#' inputs and return a list of outputs of the same length. If the elements of the output list
#' are not character vectors of length 1, they will be converted to a serialized
#' string representation via 'jsonlite'.
#' @param sample_input For a prediction function that accepts a list of inputs of type X,
#' this should be a single input of type X. This is used to validate the compatability
#' of the function with Clipper and to determine the Clipper data type (bytes, ints, strings, etc)
#' to associate with the model.
#' @param model_registry character vector of length 1. The name of the image registry
#' to which to upload the model image. If NULL, the image will not be uploaded to a registry.
build_model = function(model_name, model_version, prediction_function, sample_input, model_registry = NULL) {
  serialized_classes = c("data.frame", "matrix", "array", "list")
  input_class = class(sample_input)
  clipper_input_type <- switch(
    input_class,
    "raw" = "bytes",
    "integer" = "ints",
    "numeric" = "doubles",
    "character" = "strings",
    {
      if(!(input_class %in% serialized_classes)) {
        err_msg = 
          sprintf("Function input type of class %s is not supported", 
                  input_class)
        stop(err_msg)
      }
      "strings"
    }
  )
  
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
  
  base_model_path = "/tmp/clipper/"
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
  serialize_function(prediction_function_name, full_model_path)
  
  sample_input_path = file.path(base_model_path, relative_model_path, "sample.rds")
  saveRDS(sample_input, sample_input_path)
  
  package_path <- paste(system.file(package="Rclipper"), "build_container.py", sep="/")
  
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
  
  if(is.null(model_registry)) {
    image_name = sprintf("%s:%s", model_name, model_version)
  } else {
    image_name = sprintf("%s/%s:%s", model_registry, model_name, model_version)
  }
  
  deployment_instructions = paste(
    "To deploy this model, execute the following command from a connected ClipperConnection object `conn`:",
    sprintf("conn.deploy_model(\"%s\", \"%s\", \"%s\", \"%s\", num_replicas=<num_container_replicas>)", 
            model_name, model_version, clipper_input_type, image_name),
    "",
    sep='\n')
  
  cat(deployment_instructions)
}
