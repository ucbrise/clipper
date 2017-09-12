#' Serves a model by connecting to clipper at the address
#' specified by the provided ip and port
#'
#' @param name character vector of length 1. The name to give to the model.
#' @param version character vector of length 1. The version to give to the model.
#' @param ip character vector of length 1. The ip address of the Clipper host machine
#' @param port integer. The port of the Clipper host machine
#' @param fn function. The model's prediction function.
#' @param sample_input For a prediction function that accepts a list of inputs of type X,
#' this should be a single input of type X. This is used to validate the compatability
#' of the function with Clipper and to determine the Clipper data type (bytes, ints, strings, etc)
#' to associate with the model.
serve_model = function(name, version, ip, port, fn, sample_input) {
  input_class = class(sample_input)
  pred_fn = function(input) {
    caught_error = FALSE
    results <- tryCatch({
      fn(input)
    }, error = function(e) {
      error_output = sprintf('{"error": %s"}', e)
      caught_error <<- TRUE
      return(error_output)
    })
    if(caught_error) {
      # 'results' is the error text, so return it directly
      return(results)
    } else if(class(results) != "list") {
      err_msg = 
        sprintf("Prediction function must return a list, but
                evaluation on inputs returned an object of type
                an object of type: %s", class(results))
      return(err_msg)
    } else {
      # serialize the result before returning
      return(lapply(results, function(result) {
        if(class(result) == "character" && length(result) == 1) {
          # If the result is a single string (string vector of length 1)
          # We won't serialize it
          return(result)
        } else {
          return(jsonlite::serializeJSON(result))
        }
      }))
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
          package="Rclipper.serve")
  } else if(input_class == "integer") {
    .Call("serve_integer_vector_model",
          name,
          version,
          ip,
          port,
          pred_fn,
          package="Rclipper.serve")
  } else if(input_class == "raw") {
    .Call("serve_raw_vector_model",
          name,
          version,
          ip,
          port,
          pred_fn,
          package="Rclipper.serve")
  } else if(input_class == "character") {
    parse_string_pred_fn = function(inputs) {
      get_string = function(input) {
        # The input arrives as a list of byte
        # codes. We first map these to their
        # corresponding characters.
        char_list <- sapply(input, function(code) {
          return(rawToChar(code))
        })
        # Concatenate the list of characters
        # into a single string
        return(paste(Reduce(c, char_list), collapse=""))
      }
      return(pred_fn(lapply(inputs, get_string)))
    }
    .Call("serve_character_vector_model",
          name,
          version,
          ip,
          port,
          parse_string_pred_fn,
          package="Rclipper.serve")
  } else if(input_class %in% serialized_classes) {
    deserialize_pred_fn = function(inputs) {
      deserialize_and_predict = function(input) {
        # The input arrives as a list of byte
        # codes. We first map these to their
        # corresponding characters.
        char_list <- sapply(input, function(code) {
          return(rawToChar(code))
        })
        # Concatenate the list of characters
        # into a single string
        serialized_str = paste(Reduce(c, char_list), collapse="")
        caught_error = FALSE
        deserialized_input <- tryCatch({
          jsonlite::unserializeJSON(serialized_str)
        }, error = function(e) {
          caught_error <<- TRUE
          error_output = sprintf('{"error": %s"}', e)
          return(error_output)
        })
        if(caught_error) {
          # 'deserialized_input' is the error message, 
          # so return it directly
          return(deserialized_input)
        }
        deserialized_input_class = class(deserialized_input)
        if(deserialized_input_class != input_class) {
          return(sprintf("Received invalid input of class `%s`` 
                         for model expecting inputs of class `%s`", 
                         deserialized_input_class, 
                         input_class))
        }
        # Note: This evaluation procedure doesn't allow for 
        # batching with serialized R class inputs
        return(pred_fn(list(deserialized_input))[[1]])
      }
      return(lapply(inputs, deserialize_and_predict))
    }
    .Call("serve_serialized_input_model",
          name,
          version,
          ip,
          port,
          deserialize_pred_fn,
          package="Rclipper.serve")
  } else {
    err_msg = 
      sprintf("Function input type of class %s is not supported", 
              input_class)
    stop(err_msg)
  }
}
