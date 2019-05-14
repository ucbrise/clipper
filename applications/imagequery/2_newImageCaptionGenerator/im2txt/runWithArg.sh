#!/bin/bash
# The path to the model and checkpoint
CHECKPOINT_PATH="/container/workspace/im2txt/model/newmodel.ckpt-2000000"
# the path to the word_counts.txt
VOCAB_FILE="/container/workspace/im2txt/data/word_counts.txt"
# the image to be analyzed
IMAGE_FILE="/container/workspace/im2txt/data/images/image.jpg"
echo "CHECKPOINT_PATH: ${CHECKPOINT_PATH}"
echo "VOCAB_FILE: ${VOCAB_FILE}"
echo "IMAGE_FILE: ${IMAGE_FILE}"

python3 /container/workspace/im2txt/run_inference.py --checkpoint_path ${CHECKPOINT_PATH} --vocab_file ${VOCAB_FILE} --input_files ${IMAGE_FILE}