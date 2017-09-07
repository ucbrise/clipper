# Note: We expect sample_input to be a single input, not a list of inputs
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
  serialize_function(prediction_function_name, full_model_path)
  
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