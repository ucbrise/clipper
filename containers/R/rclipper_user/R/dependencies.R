get_library_dependencies = function(fn) {
  env_dependencies = (.packages())
  namespace_dependencies = character()
  all_dependencies = CodeDepends::getInputs(fn)
  for(i in seq_along(all_dependencies)) {
    namespace_dependencies = c(namespace_dependencies, all_dependencies[[i]]@libraries)
  }
  library_dependencies = c(env_dependencies, namespace_dependencies)
  return(unique(library_dependencies))
}

get_input_dependencies = function(fn) {
  all_input_dependencies = character()
  all_dependencies = CodeDepends::getInputs(fn)
  for(i in seq_along(all_dependencies)) {
    input_dependencies = all_dependencies[[i]]@inputs
    for(j in seq_along(input_dependencies)) {
      input_dep = input_dependencies[[j]]
      if(exists(input_dep, inherits=FALSE, envir=.GlobalEnv)) {
        # If the input object dependency is user-defined,
        # we should to serialize it
        all_input_dependencies = c(all_input_dependencies, input_dep)
      }
    }
  }
  return(all_input_dependencies)
}

get_function_dependencies = function(fn) {
  all_function_dependencies = character()
  all_dependencies = CodeDepends::getInputs(fn)
  for(i in seq_along(all_dependencies)) {
    function_dependencies = names(all_dependencies[[i]]@functions)
    for(j in seq_along(function_dependencies)) {
      func_dep = function_dependencies[[j]]
      if(exists(func_dep, inherits=FALSE, envir=.GlobalEnv)) {
        # If the function dependency is user-defined,
        # we should to serialize it
        all_function_dependencies = c(all_function_dependencies, func_dep)
      }
    }
  }
  return(all_function_dependencies)
}

get_all_dependencies = function(fn, output_path) {
  dependency_count = 0
  all_library_dependencies = character()
  all_input_dependencies = character()
  all_function_dependencies = character()
    
  get_deps = function(func) {
    lib_deps = get_library_dependencies(func)
    all_library_dependencies <<- c(all_library_dependencies, lib_deps)
    
    input_deps = get_input_dependencies(func)
    all_input_dependencies <<- c(all_input_dependencies, input_deps)
    
    func_deps = get_function_dependencies(func)
    
    for(i in seq_along(func_deps)) {
      func_name = func_deps[[i]]
      if(!(func_name %in% all_function_dependencies)) {
        # If we haven't explored this function dependency yet,
        # recursively obtain its dependencies
        get_deps(get(func_name))
      }
      all_function_dependencies <<- c(all_function_dependencies, func_name)
    }
  }
  
  get_deps(fn)
  
  all_library_dependencies = unique(all_library_dependencies)
  all_input_dependencies = unique(all_input_dependencies)
  all_function_dependencies = unique(all_function_dependencies)
  
  return(list(all_library_dependencies, all_input_dependencies, all_function_dependencies))
}

serialize_function = function(fn, output_dir_path) {
  all_dependencies = get_all_dependencies(fn)
  library_dependencies = all_dependencies[[1]]
  input_dependencies = all_dependencies[[2]]
  function_dependencies = all_dependencies[[3]]
  
  lib_out_path = file.path(output_dir_path, "libs.rds")
  saveRDS(library_dependencies, lib_out_path)
  print(paste(c("Serialized list of dependent libraries: ", library_dependencies), collapse=" "))
  
  for(i in seq_along(input_dependencies)) {
    out_path = file.path(output_dir_path, sprintf("input_dep_%d.rds", i))
    input_name = input_dependencies[i]
    save(list=input_name, file=out_path)
    print(paste(c("Serialized dependent object: ", input_name), collapse=" "))
  }
  
  for(i in seq_along(function_dependencies)) {
    out_path = file.path(output_dir_path, sprintf("fn_dep_%d.rds", i))
    function_name = function_dependencies[i]
    save(list=function_name, file=out_path)
    print(paste(c("Serialized dependent function: ", function_name), collapse=" "))
  }
  
  model_fn_name = as.character(substitute(fn))
  model_fn_out_path = file.path(output_dir_path, "fn.rds")
  saveRDS(fn, file=model_fn_out_path)
  print(paste(c("Serialized model function: ", model_fn_name), collapse= " "))
  
  print("Done!")
}