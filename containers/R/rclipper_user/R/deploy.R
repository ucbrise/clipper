deploy_model = function(model_name, model_version, model_function, sample_input, model_registry=NULL) {
  base_model_path = "/tmp/r_models/"
  dir.create(file.path(base_model_path))
  
  time_format = "%Y-%m-%d-%H%-%M-%OS"
  # Force seconds field in timestamp
  # to print to millisecond resolution
  op = options(digits.secs=3)
  
  relative_model_path = format(Sys.time(), time_format)
  
  # Reset options to their preexisting state
  options(op)
  
  full_model_path = file.path(base_model_path, relative_model_path)
  dir.create(full_model_path)
  
  model_function_name = as.character(substitute(model_function))
  serialize_function(model_function_name, full_model_path)
  
  sample_input_path = file.path(base_model_path, relative_model_path, "sample.rds")
  saveRDS(sample_input, sample_input_path)
  
  package_path <- paste(system.file(package="rclipper"), "build_container.py", sep="/")
  
  if(missing(model_registry)) {
    python_call = sprintf("python %s -m %s -n %s -v %d",
                          package_path,
                          full_model_path, 
                          model_name,
                          model_version
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