get_library_dependencies = function(cd_dependencies) {
  env_dependencies = (.packages())
  namespace_dependencies = character()
  for(i in seq_along(cd_dependencies)) {
    namespace_dependencies = c(namespace_dependencies, cd_dependencies[[i]]@libraries)
  }
  library_dependencies = c(env_dependencies, namespace_dependencies)
  return(unique(library_dependencies))
}

get_input_dependencies = function(cd_dependencies) {
  all_input_dependencies = character()
  for(i in seq_along(cd_dependencies)) {
    input_dependencies = cd_dependencies[[i]]@inputs
    for(j in seq_along(input_dependencies)) {
      input_dep = input_dependencies[[j]]
      if(exists(input_dep, inherits=FALSE, envir=.GlobalEnv)) {
        # If the input object dependency is user-defined,
        # we should serialize it
        all_input_dependencies = c(all_input_dependencies, input_dep)
      }
    }
  }
  return(all_input_dependencies)
}

get_file_dependencies = function(cd_dependencies) {
  # This is a bit hacky. `CodeDepends` doesn't always
  # return the same data type. Either a class extending
  # `list` or a class of type `S4` is returned. In the
  # second case, it's sufficient to coerce the object to a list.
  if(typeof(cd_dependencies) == "S4") {
    cd_dependencies = list(cd_dependencies)
  }
  all_file_dependencies = character()
  for(i in seq_along(cd_dependencies)) {
    file_dependencies = cd_dependencies[[i]]@files
    all_file_dependencies = c(all_file_dependencies, file_dependencies)
  }
  return(all_file_dependencies)
}

get_function_dependencies = function(cd_dependencies) {
  all_function_dependencies = character()
  for(i in seq_along(cd_dependencies)) {
    function_dependencies = names(cd_dependencies[[i]]@functions)
    for(j in seq_along(function_dependencies)) {
      func_dep = function_dependencies[[j]]
      if(exists(func_dep, inherits=FALSE, envir=.GlobalEnv)) {
        # If the function dependency is user-defined,
        # we should serialize it
        all_function_dependencies = c(all_function_dependencies, func_dep)
      }
    }
  }
  return(all_function_dependencies)
}

get_all_dependencies = function(fn_name, output_path) {
  dependency_count = 0
  all_library_dependencies = character()
  all_file_dependencies = list()
  all_input_dependencies = character()
  all_function_dependencies = character()
    
  get_deps = function(func_name) {
    func = get(func_name)
    
    cd_dependencies <- tryCatch({
      CodeDepends::getInputs(func)
    }, error = function(e) {
      print(e)
      stop("CodeDepends encountered an error while analyzing the model function!")
    })
    
    lib_deps = get_library_dependencies(cd_dependencies)
    all_library_dependencies <<- c(all_library_dependencies, lib_deps)
    
    func_file_deps = get_file_dependencies(cd_dependencies)
    if(length(func_file_deps) > 0) {
      # We can't assign a dictionary key to an empty vector,
      # so only make the assignment if we found file dependencies
      all_file_dependencies[[func_name]] <<- func_file_deps
    }
    
    input_deps = get_input_dependencies(cd_dependencies)
    all_input_dependencies <<- c(all_input_dependencies, input_deps)
    
    for(i in seq_along(input_deps)) {
      input_name = input_deps[i]
      input = get(input_name)
      input_cd_deps = CodeDepends::getInputs(input)
      input_file_deps = get_file_dependencies(input_cd_deps)
      if(length(input_file_deps) > 0) {
        # We can't assign a dictionary key to an empty vector,
        # so only make the assignment if we found file dependencies
        all_file_dependencies[[input_name]] <<- input_file_deps
      }
    }
    
    func_deps = get_function_dependencies(cd_dependencies)
    for(i in seq_along(func_deps)) {
      dep_func_name = func_deps[i]
      if(!(dep_func_name %in% all_function_dependencies)) {
        # If we haven't explored this function dependency yet,
        # recursively obtain its dependencies
        get_deps(dep_func_name)
      }
      all_function_dependencies <<- c(all_function_dependencies, dep_func_name)
    }
  }
  
  get_deps(fn_name)
  
  all_library_dependencies = unique(all_library_dependencies)
  all_input_dependencies = unique(all_input_dependencies)
  all_function_dependencies = unique(all_function_dependencies)
  
  return(list(all_library_dependencies, all_file_dependencies, all_input_dependencies, all_function_dependencies))
}

serialize_function = function(fn_name, output_dir_path) {
  log_step = function(description, object_name) {
    print(paste(c(description, object_name), collapse=": "))
  }
  
  all_dependencies = get_all_dependencies(fn_name)
  library_dependencies = all_dependencies[[1]]
  file_dependencies = all_dependencies[[2]]
  input_dependencies = all_dependencies[[3]]
  function_dependencies = all_dependencies[[4]]
  
  lib_out_path = file.path(output_dir_path, "libs.rds")
  saveRDS(library_dependencies, lib_out_path)
  log_step("Serialized list of dependent libraries", library_dependencies)
  
  for(i in seq_along(input_dependencies)) {
    out_path = file.path(output_dir_path, sprintf("input_dep_%d.rds", i))
    input_name = input_dependencies[i]
    save(list=input_name, file=out_path)
    log_step("Serialized dependent object", input_name)
  }
  
  for(i in seq_along(function_dependencies)) {
    function_out_path = file.path(output_dir_path, sprintf("fn_dep_%d.rds", i))
    function_name = function_dependencies[i]
    save(list=function_name, file=function_out_path)
    log_step("Serialized dependent function", function_name)
  }
  
  # Given a file path, this can be used to match its extension (including '.' character)
  file_extension_pattern = "(\\.[^.]+)$"
  file_dependency_count = 0
  file_dependent_object_names = names(file_dependencies)
  
  # A mapping from a function name to a list of
  # tuples (2 element vector). Each tuple contains
  # the original path of a file dependency and its
  # new name after copying
  object_file_dependency_map = list()
  
  all_dependent_objects = c(function_dependencies, fn_name, input_dependencies)
  for(i in seq_along(all_dependent_objects)) {
    obj_name = all_dependent_objects[i]
    if(obj_name %in% file_dependent_object_names) {
      obj_file_deps = file_dependencies[[obj_name]]
      obj_file_map_entries = list()
      for(i in seq_along(obj_file_deps)) {
        file_dep_path = obj_file_deps[i]
        if(dir.exists(file_dep_path)) {
          # The dependency is a directory containing multiple files. Recursively copy it.
          dest_dir_name = sprintf("dir_%d", file_dependency_count)
          dest_dir_path = file.path(output_dir_path, dest_dir_name)
          dir.create(dest_dir_path)
          file.copy(file.path(file_dep_path, "."), dest_dir_path, recursive=TRUE)
          obj_file_map_entries[[i]] = c(file_dep_path, dest_dir_name)
          log_step("Recursively copied dependent directory", file_dep_path)
        } else {
          # The dependency is a single file. Copy it, preserving its extension.
          file_dep_extension = stringr::str_extract(file_dep_path, file_extension_pattern)
          if(is.na(file_dep_extension)) {
            # It's possible that the file has no extension. 
            # This avoids incorrectly appending `NA` to the file
            file_dep_extension = ""
          }
          dest_file_name = sprintf("file_%d%s", file_dependency_count, file_dep_extension)
          dest_file_path = file.path(output_dir_path, dest_file_name)
          file.copy(file_dep_path, dest_file_path)
          obj_file_map_entries[[i]] = c(file_dep_path, dest_file_name)
          log_step("Copied dependent file", file_dep_path)
        }
        file_dependency_count = file_dependency_count + 1
      }
      object_file_dependency_map[[obj_name]] = obj_file_map_entries
    }
  }
  
  object_files_map_out_path = file.path(output_dir_path, "obj_files_map.rds")
  saveRDS(object_file_dependency_map, object_files_map_out_path)
  
  model_fn_out_path = file.path(output_dir_path, "fn.rds")
  save(list=fn_name, file=model_fn_out_path)
  log_step("Serialized model function", fn_name)
  
  print("Done!")
}
