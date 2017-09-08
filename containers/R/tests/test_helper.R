create_output_directory = function() {
  base_output_path = "/tmp/r_test_models/"
  dir.create(file.path(base_output_path), showWarnings = FALSE)
  
  time_format = "%Y-%m-%d-%H%-%M-%OS"
  # Force seconds field in timestamp
  # to print to millisecond resolution
  op = options(digits.secs=3)
  
  relative_output_path = format(Sys.time(), time_format)
  
  # Reset options to their preexisting state
  options(op)
  
  full_output_path = file.path(base_output_path, relative_output_path)
  dir.create(full_output_path)
  
  return(full_output_path)
}