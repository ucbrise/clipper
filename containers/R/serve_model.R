#!/usr/bin/env Rscript
library("optparse")
library("stringr")

option_list = list(
  make_option(c("-m", "--model_data_path"), type="character", default=NULL, 
              help="path to serialized model and dependencies"),
  make_option(c("-n", "--model_name"), type="character", default=NULL, 
              help="model name"),
  make_option(c("-v", "--model_version"), type="character", default=NULL, 
              help="model version"),
  make_option(c("-i", "--clipper_ip"), type="character", default=NULL, 
              help="clipper host ip"),
  make_option(c("-p", "--clipper_port"), type="character", default=NULL, 
              help="clipper host rpc port")
);

opt_parser = OptionParser(option_list=option_list);
opts = parse_args(opt_parser);

lib_deps <- tryCatch({
  libs_path = file.path(opts$model_data_path, "libs.rds")
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
  library(lib_dep, character.only=TRUE)
}

sample_input <- tryCatch({
  sample_input_path = file.path(opts$model_data_path, "sample.rds")
  readRDS(sample_input_path)
}, error = function(e) {
  print(e)
  stop("Failed to load sample input")
})
model_input_type = class(sample_input)

model_function_info <- tryCatch({
  model_path = file.path(opts$model_data_path, "fn.rds")
  load(model_path)
}, error = function(e) {
  print(e)
  stop("Failed to load model function!")
})
model_function_name = model_function_info[[1]]

depfile_pattern = "dep_[0-9]+.rds"

model_data_file_names = list.files(opts$model_data_path)
for(i in seq_along(model_data_file_names)) {
  file_name = model_data_file_names[i]
  if(!is.na(str_extract(file_name, depfile_pattern))) {
    dep_load_path = file.path(opts$model_data_path, file_name)
    tryCatch({
      load(dep_load_path)
    }, error = function(e) {
      print(e)
      stop(paste(c("Failed to load dependency ", file_name), collapse=" "))
    })
  }
}

func_file_dependency_map <- tryCatch({
  map_path = file.path(opts$model_data_path, "func_files_map.rds")
  readRDS(map_path)
}, error = function(e) {
  print(e)
  stop("Failed to load function-to-files dependency mapping")
})

file_dependent_func_names = names(func_file_dependency_map)
for(i in seq_along(file_dependent_func_names)) {
  func_name = file_dependent_func_names[i]
  func_file_deps = func_file_dependency_map[[i]]
  for(j in seq_along(func_file_deps)) {
    original_dep_path = func_file_deps[[j]][1]
    new_dep_name = func_file_deps[[j]][2]
    new_dep_path = file.path(opts$model_data_path, new_dep_name)
    
    # Convert the original function into a list containing its
    # lines of code
    original_func_string = deparse(get(func_name))
    
    # Replace all code occurrences of the old file dependency path
    # with the path to the copied dependency included within
    # the model data directory
    new_func_lines = lapply(original_func_string, function(str) {
      return(gsub(original_dep_path, new_dep_path, str))
    })
    new_func_string = paste(new_func_lines, collapse="\n")
    
    # Convert the new code back into a function
    new_func = eval(parse(text=new_func_string))
    
    # Reassign the variable referencing the original function 
    # to reference the new one
    assign(func_name, new_func)
  }
}

rclipper::serve_model(opts$model_name, strtoi(opts$model_version), opts$clipper_ip, 
                      strtoi(opts$clipper_port), get(model_function_name), model_input_type)