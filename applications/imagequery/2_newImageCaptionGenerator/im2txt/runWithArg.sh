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

find / -name run_inference.py
python /container/im2txt/run_inference.py --checkpoint_path ${CHECKPOINT_PATH} --vocab_file ${VOCAB_FILE} --input_files ${IMAGE_FILE}