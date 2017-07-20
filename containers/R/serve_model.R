#!/usr/bin/env Rscript
library("optparse")

option_list = list(
  make_option(c("-m", "--model_path"), type="character", default=NULL, 
              help="serialized model path"),
  make_option(c("-s", "--sample_input_path"), type="character", default=NULL, 
              help="sample input path"),
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
opt = parse_args(opt_parser);

model = readRDS(opt$model_path)
sample_input = readRDS(opt$sample_input_path)

rclipper::serve_model(opt$model_name, strtoi(opt$model_version), opt$clipper_ip, 
                      strtoi(opt$clipper_port), model, sample_input)