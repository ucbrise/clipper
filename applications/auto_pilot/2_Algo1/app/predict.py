import tensorflow as tf
import model
import json
import scipy.misc

sess = tf.InteractiveSession()
saver = tf.train.Saver()
saver.restore(sess, "/container/model.ckpt")

def predict(image_str):
    image = json.loads(image_str)
    image = scipy.misc.imresize(image[-150:], [66, 200]) / 255.0
    degrees = model.y.eval(feed_dict={model.x: [image], model.keep_prob: 1.0})[0][0] * 180.0 / 3.1415926
    return str(degrees)
