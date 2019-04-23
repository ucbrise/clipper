#!/bin/bash
# The path to the model and checkpoint
CHECKPOINT_PATH=$1
# the path to the word_counts.txt
VOCAB_FILE=$2
# the image to be analyzed
IMAGE_FILE=$3
echo "CHECKPOINT_PATH: $1"
echo "VOCAB_FILE: $2"
echo "IMAGE_FILE: $3"
# bazel compilation
# go to the workspace
cd /container/workspace/
# //im2txt:run_inference means: go to my workspace, go to im2txt and execute run_inference
bazel build -c opt //im2txt:run_inference
# compile bazel with parameters
bazel-bin/im2txt/run_inference \
  --checkpoint_path=${CHECKPOINT_PATH} \
  --vocab_file=${VOCAB_FILE} \
  --input_files=${IMAGE_FILE}