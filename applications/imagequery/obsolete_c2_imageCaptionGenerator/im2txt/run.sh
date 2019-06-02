# The path to the model and checkpoint
CHECKPOINT_PATH="C:/Users/musicman/Desktop/researchCode/2_imageCaptionGenerator/im2txt/model/newmodel.ckpt-2000000" # replace this with your own file path to the model 
# the path to the word_counts.txt
VOCAB_FILE="C:/Users/musicman/Desktop/researchCode/2_imageCaptionGenerator/im2txt/data/word_counts.txt" # replace this with your own file path to the word_counts.txt
# the image to be analyzed
IMAGE_FILE="C:/Users/musicman/Desktop/tianhao/clipper/applications/imageQuery/1_imageResizer/resized_1.jpg" # replace this with your own image to be captioned

# bazel compilation
bazel build -c opt im2txt/run_inference

# compile bazel with parameters
bazel-bin/im2txt/run_inference \
  --checkpoint_path=${CHECKPOINT_PATH} \
  --vocab_file=${VOCAB_FILE} \
  --input_files=${IMAGE_FILE}