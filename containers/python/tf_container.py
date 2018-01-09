from __future__ import print_function
import rpc
import os
import sys
import tensorflow as tf
import cloudpickle
import glob


def load_predict_func(file_path):
    with open(file_path, 'r') as serialized_func_file:
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
            with tf.gfile.GFile(frozen_graph_exists[0], "rb") as f:
                graph_def = tf.GraphDef()
                graph_def.ParseFromString(f.read())

            with tf.Graph().as_default() as graph:
                tf.import_graph_def(
                    graph_def, input_map=None, return_elements=None, name="")
            self.sess = tf.Session(graph=graph)
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
    try:
        model_name = os.environ["CLIPPER_MODEL_NAME"]
    except KeyError:
        print(
            "ERROR: CLIPPER_MODEL_NAME environment variable must be set",
            file=sys.stdout)
        sys.exit(1)
    try:
        model_version = os.environ["CLIPPER_MODEL_VERSION"]
    except KeyError:
        print(
            "ERROR: CLIPPER_MODEL_VERSION environment variable must be set",
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
    if "CLIPPER_INPUT_TYPE" in os.environ:
        input_type = os.environ["CLIPPER_INPUT_TYPE"]
    else:
        print("Using default input type: doubles")

    model_dir_path = os.environ["CLIPPER_MODEL_PATH"]
    model_files = os.listdir(model_dir_path)
    assert len(model_files) >= 2
    fname = os.path.splitext(model_files[0])[0]
    full_fname = os.path.join(model_dir_path, fname)
    model = TfContainer(model_dir_path, input_type)
    rpc_service = rpc.RPCService()
    rpc_service.start(model, ip, port, model_name, model_version, input_type)
