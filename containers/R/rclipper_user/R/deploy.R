serve_model = function(model_name, model_version, clipper_ip, clipper_port, model_function, sample_input) {
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
  
  serialize_function(model_function, full_model_path)
  
  sample_input_path = file.path(base_model_path, relative_model_path, "sample.rds")
  saveRDS(sample_input, sample_input_path)
  
  # CALL PYTHON SCRIPT
}