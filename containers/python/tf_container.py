from __future__ import print_function
import rpc
import os
import sys
import tensorflow as tf
import cloudpickle
import glob

from tensorflow.python.saved_model import loader

IMPORT_ERROR_RETURN_CODE = 3


def load_predict_func(file_path):
    if sys.version_info < (3, 0):
        with open(file_path, 'r') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)
    else:
        with open(file_path, 'rb') as serialized_func_file:
            return cloudpickle.load(serialized_func_file)


class TfContainer(rpc.ModelContainerBase):
    def __init__(self, path, input_type):
        self.input_type = rpc.string_to_input_type(input_type)
        modules_folder_path = "{dir}/modules/".format(dir=path)
        sys.path.append(os.path.abspath(modules_folder_path))
        predict_fname = "func.pkl"
        predict_path = "{dir}/{predict_fname}".format(
            dir=path, predict_fname=predict_fname)
        self.predict_func = load_predict_func(predict_path)

        frozen_graph_exists = glob.glob(os.path.join(path, "tfmodel/*.pb"))
        if (len(frozen_graph_exists) > 0):
            with tf.Graph().as_default() as graph:
                self.sess = tf.Session(graph=graph)
                loader.load(self.sess, [tf.saved_model.tag_constants.SERVING],
                            os.path.join(path, "tfmodel"))
        else:
            self.sess = tf.Session(
                '',
                tf.Graph(),
                config=tf.ConfigProto(
                    allow_soft_placement=True, log_device_placement=True))
            metagraph_path = glob.glob(os.path.join(path, "tfmodel/*.meta"))[0]
            checkpoint_path = metagraph_path.split(".meta")[0]
            with tf.device("/gpu:0"):
                with self.sess.graph.as_default():
                    saver = tf.train.import_meta_graph(
                        metagraph_path, clear_devices=True)
                    saver.restore(self.sess, checkpoint_path)

    def predict_ints(self, inputs):
        preds = self.predict_func(self.sess, inputs)
        return [str(p) for p in preds]

    def predict_floats(self, inputs):
        preds = self.predict_func(self.sess, inputs)
        return [str(p) for p in preds]

    def predict_doubles(self, inputs):
        preds = self.predict_func(self.sess, inputs)
        return [str(p) for p in preds]

    def predict_bytes(self, inputs):
        preds = self.predict_func(self.sess, inputs)
        return [str(p) for p in preds]

    def predict_strings(self, inputs):
        preds = self.predict_func(self.sess, inputs)
        return [str(p) for p in preds]


if __name__ == "__main__":
    print("Starting TensorFlow container")
    rpc_service = rpc.RPCService()
    try:
        model = TfContainer(rpc_service.get_model_path(),
                            rpc_service.get_input_type())
        sys.stdout.flush()
        sys.stderr.flush()
    except ImportError:
        sys.exit(IMPORT_ERROR_RETURN_CODE)
    rpc_service.start(model)
