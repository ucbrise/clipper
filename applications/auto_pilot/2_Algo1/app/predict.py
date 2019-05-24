import tensorflow as tf
import model
import json
import scipy.misc

sess = tf.InteractiveSession()
saver = tf.train.Saver()
saver.restore(sess, "/container/model.ckpt")

def predict(image_str):
	fh = open("temp.jpg", "wb")
	fh.write(base64.decodebytes(image_str))
	fh.close()
    image = scipy.misc.imread("temp.jpg", mode="RGB")
    image = scipy.misc.imresize(image[-150:], [66, 200]) / 255.0
    degrees = model.y.eval(feed_dict={model.x: [image], model.keep_prob: 1.0})[0][0] * 180.0 / 3.1415926
    return str(degrees)
