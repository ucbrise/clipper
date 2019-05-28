from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from timeit import default_timer as timer


import math
import os
import json
import tensorflow as tf

import configuration
import inference_wrapper

import caption_generator
import vocabulary


def find(name, path):
    for root, dirs, files in os.walk(path):
        if name in files or name in dirs:
            return os.path.join(root, name)


# Run run_inference.py, here the image.jpg is just the testing image for setting up the tensorflow environment
run_inference_path = find("run_inference.py", "/")
model_dir_path = find("model", "/container/im2txt")
checkpoint_path = model_dir_path + "/newmodel.ckpt-2000000"
vocabulary_path = find("word_counts.txt", "/")
image_path = find("image.jpg", "/")
#setupTensorflowEnvironmentCmd = "python " + run_inference_path + " --checkpoint_path " + check_point_path + " --vocab_file " + vocabulary_path + " --input_files " + image_path
# os.system(setupTensorflowEnvironmentCmd)


FLAGS = tf.flags.FLAGS
#tf.flags.DEFINE_string("checkpoint_path", "", "Model checkpoint file or directory containing a model checkpoint file.")
#tf.flags.DEFINE_string("vocab_file", "", "Text file containing the vocabulary.")
#tf.flags.DEFINE_string("input_files", "", "File pattern or comma-separated list of file patterns of image files.")

tf.logging.set_verbosity(tf.logging.INFO)

# Build the inference graph.
g = tf.Graph()
with g.as_default():
    model = inference_wrapper.InferenceWrapper()
    restore_fn = model.build_graph_from_config(
        configuration.ModelConfig(), checkpoint_path)
g.finalize()

# Create the vocabulary.
vocab = vocabulary.Vocabulary(vocabulary_path)
#filenames = []
#for file_pattern in input_files.split(","):
#    filenames.extend(tf.gfile.Glob(file_pattern))
#tf.logging.info("Running caption generation on %d files matching %s", len(
#    filenames), FLAGS.input_files)

print("Finished building inference graph and creating vocabulary list...")

sess = tf.Session(graph=g) 
# Load the model from checkpoint.
restore_fn(sess)

# Prepare the caption generator.
generator = caption_generator.CaptionGenerator(model, vocab)

   


def predict(image_file_index):

    image_file_index = int(image_file_index)
    if image_file_index > 800:
        return "Invalid image file index! Only index between 1 to 800 is allowed!"

    image_file_path = "/container/im2txt/data/imageDataset/101_ObjectCategories/" + str(image_file_index) + ".jpg"

    # added by YIN Yue
    captionList = ["", "", ""]
    with tf.gfile.GFile(image_file_path, "rb") as f:
        image = f.read()
        captions = generator.beam_search(sess, image)
        for i, caption in enumerate(captions):
            # Ignore begin and end words.
            sentence = [vocab.id_to_word(w) for w in caption.sentence[1:-1]]
            sentence = " ".join(sentence)
            captionList[i] = sentence
            print("  %d) %s (p=%f)" % (i, sentence, math.exp(caption.logprob)))
            # the end of caption generation

    # generated_caption = ' '.join(captionList)
    # return only the one with the highest probability
    generated_caption = captionList[0]

    return generated_caption


if __name__ == "__main__":
    print(predict(1))
    print(predict(2))

