#!/usr/bin/env Rscript
library("optparse")

source("deserialize_model.R")

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

sample_input <- tryCatch({
  sample_input_path = file.path(opts$model_data_path, "sample.rds")
  readRDS(sample_input_path)
}, error = function(e) {
  print(e)
  stop("Failed to load sample input")
})

model_function_name = deserialize_model(opts$model_data_path)

Rclipper.serve::serve_model(opts$model_name, strtoi(opts$model_version), opts$clipper_ip, 
                      strtoi(opts$clipper_port), get(model_function_name), sample_input)
