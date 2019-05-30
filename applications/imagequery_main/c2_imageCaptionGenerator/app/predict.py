from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from timeit import default_timer as timer
load_start = timer()

import math
import os
import json
import tensorflow as tf

print(os.chdir)

import configuration
import inference_wrapper
import caption_generator
import vocabulary


def find(name, path):
    for root, dirs, files in os.walk(path):
        if name in files or name in dirs:
            return os.path.join(root, name)

# model_dir_path = find("model", "/container/im2txt")
# checkpoint_path = model_dir_path + "/newmodel.ckpt-2000000"
# vocabulary_path = find("word_counts.txt", "/")
checkpoint_path = "/container/im2txt/model/newmodel.ckpt-2000000"
vocabulary_path = "/container/im2txt/data/word_counts.txt"

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

sess = tf.Session(graph=g) 
# Load the model from checkpoint.
restore_fn(sess)

# Prepare the caption generator.
generator = caption_generator.CaptionGenerator(model, vocab)

load_end = timer()
print("Finish preloading modules in " + str(load_end - load_start) + " seconds!")
   


def predict(image_file_index):
    start = timer()

    image_file_index = int(image_file_index)
    if image_file_index > 1000:
        return "Invalid image file index! Only index between 1 to 1000 is allowed!"

    image_file_path = "/container/im2txt/data/imageDataset/101_ObjectCategories/" + str(image_file_index) + ".jpg"

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

    end = timer()
    time_elapsed = end - start
    print("  The image file takes " + str(time_elapsed) + " seconds")

    return generated_caption


if __name__ == "__main__":
    print(predict(1))
    print(predict(2))
    print(predict(3))
    print(predict(4))

