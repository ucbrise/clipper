from __future__ import print_function
import rpc
import os
import sys
import numpy as np
import tensorflow as tf

classes = ['airplane', 'automobile', 'bird', 'cat',
           'deer', 'dog', 'frog', 'horse', 'ship', 'truck']
positive_class = classes.index('airplane')
negative_class = classes.index('bird')

class TfCifarContainer(rpc.ModelContainerBase):
    def __init__(self, path):
        self.sess = tf.Session('', tf.Graph())
        with self.sess.graph.as_default():
            saver = tf.train.import_meta_graph(path + '.meta')
            saver.restore(self.sess, path)

    def predict_floats(self, inputs):
        logits = self.sess.run('softmax_logits:0', feed_dict={'x:0': inputs})
        relevant_activations = logits[:, [negative_class, positive_class]]
        preds = np.argmax(relevant_activations, axis=1)
        return preds

if __name__ == "__main__":
    try:
        model_name = os.environ["CLIPPER_MODEL_NAME"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_NAME environment variable must be set", file=sys.stderr)
        sys.exit(1)
    try:
        model_version = os.environ["CLIPPER_MODEL_VERSION"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_VERSION environment variable must be set", file=sys.stderr)
        sys.exit(1)

    ip = "127.0.0.1"
    if "CLIPPER_IP" in os.environ:
        ip = os.environ["CLIPPER_IP"]
    else:
        print("Connecting to Clipper on localhost")

    port = 7000
    if "CLIPPER_PORT" in os.environ:
        port = int(os.environ["CLIPPER_PORT"])
    else:
        print("Connecting to Clipper with default port: 7000")

    input_type = "floats"
    model_path = os.environ["CLIPPER_MODEL_PATH"]
    model = TfCifarContainer(model_path)
    rpc.start(model, ip, port, model_name, model_version, input_type)
