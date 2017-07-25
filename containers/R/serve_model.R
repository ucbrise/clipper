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

model_function <- tryCatch({
  model_path = file.path(opts$model_data_path, "fn.rds")
  readRDS(model_path)
}, error = function(e) {
  print(e)
  stop("Failed to load model function!")
})

depfile_regex = "dep_[0-9]+.rds"

model_data_file_names = list.files(opts$model_data_path)
for(i in seq_along(model_data_file_names)) {
  file_name = model_data_file_names[i]
  if(!is.na(str_extract(file_name, depfile_regex))) {
    dep_load_path = file.path(opts$model_data_path, file_name)
    tryCatch({
      load(dep_load_path)
    }, error = function(e) {
      print(e)
      stop(paste(c("Failed to load dependency ", file_name), collapse=" "))
    })
  }
}

rclipper::serve_model(opts$model_name, strtoi(opts$model_version), opts$clipper_ip, 
                      strtoi(opts$clipper_port), model_function, model_input_type)