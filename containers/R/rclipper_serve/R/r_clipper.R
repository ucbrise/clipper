serve_model = function(name, version, ip, port, fn, input_class) {
  pred_fn = function(input) {
    caught_error = FALSE
    result <- tryCatch({
      fn(input)
    }, error = function(e) {
      error_output = sprintf('{"error": %s"}', e)
      caught_error <<- TRUE
      return(error_output)
    })
    if(caught_error) {
      # 'result' is the error text, so return it directly
      return(result)
    } else {
     # serialize the result before returning
      return(jsonlite::serializeJSON(result)) 
    }
  }
  # Clipper requires that inputs of these classes be sent as 
  # jsonlite-serialized strings. We will then deserialize them
  # and pass them to the provided prediction function.
  serialized_classes = c("data.frame", "matrix", "array", "list")
  if(input_class == "numeric") {
    .Call("serve_numeric_vector_model",
          name,
          version,
          ip,
          port,
          pred_fn,
          package="rclipper")
  } else if(input_class == "integer") {
    .Call("serve_integer_vector_model",
          name,
          version,
          ip,
          port,
          pred_fn,
          package="rclipper")
  } else if(input_class == "raw") {
    .Call("serve_raw_vector_model",
          name,
          version,
          ip,
          port,
          pred_fn,
          package="rclipper")
  } else if(input_class %in% serialized_classes) {
    deserialize_pred_fn = function(input) {
      caught_error = FALSE
      deserialized_input <- tryCatch({
        jsonlite::unserializeJSON(input)
      }, error = function(e) {
        caught_error <<- TRUE
        error_output = sprintf('{"error": %s"}', e)
        return(error_output)
      })
      if(caught_error) {
        # 'deserialized_input' is the error message, 
        # so return it directly
        return(deserialized_pred_fn)
      }
      input_class = class(deserialized_input)
      if(input_class != input_class) {
        return(sprintf("Received invalid input of class `%s`` for model expecting inputs of class `%s`", 
                       input_class, 
                       input_class))
      }
      return(pred_fn(deserialized_input))
    }
    .Call("serve_serialized_input_model",
          name,
          version,
          ip,
          port,
          deserialize_pred_fn,
          package="rclipper")
  } else {
    err_msg = 
      sprintf("Function input type of class %s is not supported", 
              input_class)
    stop(err_msg)
  }
}
