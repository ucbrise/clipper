serve_model = function(name, version, ip, port, fn, sample_input) {
  input_class = class(sample_input)
  strfn = function(input) { return(jsonlite::serializeJSON(fn(input))) }
  if(input_class == "numeric") {
    .Call("serve_numeric_vector_model",
          name,
          version,
          ip,
          port,
          strfn,
          package="rclipper")
  } else if(input_class == "integer") {
    .Call("serve_integer_vector_model",
          name,
          version,
          ip,
          port,
          strfn,
          package="rclipper")
  } else if(input_class == "raw") {
    .Call("serve_raw_vector_model",
          name,
          version,
          ip,
          port,
          strfn,
          package="rclipper")
  } else if(input_class == "data.frame") {
    dframe_fn = function(input) { 
      data_frame <- tryCatch({
        jsonlite::unserializeJSON(input)
      }, error = function(e) {
        return(e)
      })
      #print(data_frame)
      #print(typeof(data_frame))
      return(strfn(data_frame))
    }
    .Call("serve_data_frame_model",
          name,
          version,
          ip,
          port,
          dframe_fn,
          package="rclipper")
  } else {
    err_msg = 
      sprintf("Function input type of class %s is unsupported", 
              input_class)
    stop(err_msg)
  }
}
