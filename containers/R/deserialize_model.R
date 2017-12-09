#!/usr/bin/env Rscript
library("stringr")

#' Given a path to a model serialized via the `Rclipper` user library,
#' deserializes the model and loads all of its dependencies into
#' the global environment.
#' 
#' @return The name of the deserialized model prediction function.
deserialize_model = function(model_data_path) {
  lib_deps <- tryCatch({
    libs_path = file.path(model_data_path, "libs.rds")
    readRDS(libs_path)
  }, error = function(e) {
    print(e)
    stop("Failed to load libraries list!")
  })
  for(i in seq_along(lib_deps)) {
    lib_dep = lib_deps[i]
    if(!(lib_dep %in% installed.packages())) {
      install.packages(lib_dep)
    }
    tryCatch({
      library(lib_dep, character.only=TRUE)
    }, error = function(e) {
      print(paste(c("Failed to install library:", lib_dep), collapse=" "))
      print(paste("Proceeding with deserialization.",
                  "Model inference may fail if",
                  "this library is required!", sep=" "))
    })
  }
  
  model_function_info <- tryCatch({
    model_path = file.path(model_data_path, "fn.rds")
    load(model_path)
  }, error = function(e) {
    print(e)
    stop("Failed to load model function!")
  })
  model_function_name = model_function_info[[1]]
  assign(model_function_name, get(model_function_name), .GlobalEnv)
  
  depfile_pattern = "dep_[0-9]+.rds"
  
  # Loads all of the model's dependent objects, inputs,
  # and functions into the global environment
  model_data_file_names = list.files(model_data_path)
  for(i in seq_along(model_data_file_names)) {
    file_name = model_data_file_names[i]
    if(!is.na(str_extract(file_name, depfile_pattern))) {
      dep_load_path = file.path(model_data_path, file_name)
      dep_info <- tryCatch({
        load(dep_load_path)
      }, error = function(e) {
        print(e)
        stop(paste(c("Failed to load dependency ", file_name), collapse=" "))
      })
      dep_name = dep_info[[1]]
      # Assign the dependency to its original, pre-serialization
      # name within the global environment
      assign(dep_name, get(dep_name), .GlobalEnv)
    }
  }
  
  object_file_dependency_map <- tryCatch({
    map_path = file.path(model_data_path, "obj_files_map.rds")
    readRDS(map_path)
  }, error = function(e) {
    print(e)
    stop("Failed to load function-to-files dependency mapping")
  })
  
  # For each object with a file dependency, associate the object
  # with the copied, renamed file within the global environment
  file_dependent_object_names = names(object_file_dependency_map)
  for(i in seq_along(file_dependent_object_names)) {
    obj_name = file_dependent_object_names[i]
    obj_file_deps = object_file_dependency_map[[i]]
    for(j in seq_along(obj_file_deps)) {
      original_dep_path = obj_file_deps[[j]][1]
      new_dep_name = obj_file_deps[[j]][2]
      new_dep_path = file.path(model_data_path, new_dep_name)
      
      # Convert the original object into a list containing its
      # lines of code
      original_obj_string = deparse(get(obj_name))
      
      # Replace all code occurrences of the old file dependency path
      # with the path to the copied dependency included within
      # the model data directory
      new_obj_lines = lapply(original_obj_string, function(str) {
        return(gsub(original_dep_path, new_dep_path, str))
      })
      new_obj_string = paste(new_obj_lines, collapse="\n")
      
      # Convert the new code back into an object
      new_obj = eval(parse(text=new_obj_string))
      
      # Reassign the variable referencing the original object
      # to reference the new one
      assign(obj_name, new_obj, .GlobalEnv)
    }
  }
  return(model_function_name)
}