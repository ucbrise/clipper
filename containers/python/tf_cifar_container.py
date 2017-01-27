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

    def predict_doubles(self, inputs):
        logits = self.sess.run('softmax_logits:0',
                               feed_dict={'x:0': inputs})
        relevant_activations = logits[:, [negative_class, positive_class]]
        preds = np.argmax(relevant_activations, axis=1)
        preds[preds == 0] = -1.0
        return preds.astype(np.float32)

    # def predict_ints(self, inputs):
    #     mean, sigma = np.mean(inputs, axis=1), np.std(inputs, axis=1)
    #     np.place(sigma, sigma == 0, 1.)
    #     normalized_inputs = np.transpose((inputs.T - mean) / sigma)
    #     logits = self.sess.run('softmax_logits:0',
    #                            feed_dict={'x:0': normalized_inputs})
    #     relevant_activations = logits[:, [negative_class, positive_class]]
    #     preds = np.argmax(relevant_activations, axis=1)
    #     preds[preds == 0] = -1.0
    #     return preds.astype(np.float32)


if __name__ == "__main__":
    print("Starting TensorFlow Cifar container")
    try:
        model_name = os.environ["CLIPPER_MODEL_NAME"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_NAME environment variable must be set",
              file=sys.stdout)
        sys.exit(1)
    try:
        model_version = os.environ["CLIPPER_MODEL_VERSION"]
    except KeyError:
        print("ERROR: CLIPPER_MODEL_VERSION environment variable must be set",
              file=sys.stdout)
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

    input_type = "doubles"
    model_dir_path = os.environ["CLIPPER_MODEL_PATH"]
    # we expect exactly 2 files with the same filename,
    # one with the file extension *.meta, the other
    # with no extension
    model_files = os.listdir(model_dir_path)
    assert len(model_files) == 2
    fname = os.path.splitext(model_files[0])[0]
    full_fname = os.path.join(model_dir_path, fname)
    print(full_fname)
    model = TfCifarContainer(full_fname)
    rpc.start(model, ip, port, model_name, model_version, input_type)
