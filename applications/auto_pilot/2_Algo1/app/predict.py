import tensorflow as tf
import model
import scipy.misc

sess = tf.InteractiveSession()
saver = tf.train.Saver()
saver.restore(sess, "/container/model.ckpt")

def read_image(i):
	image_path = "/container/dataset/" + i + ".jpg"
	image = scipy.misc.imread(image_path, mode="RGB").tolist()
	return image

def predict(i):
	image = read_image(i)
    image = scipy.misc.imresize(image[-150:], [66, 200]) / 255.0
    degrees = model.y.eval(feed_dict={model.x: [image], model.keep_prob: 1.0})[0][0] * 180.0 / 3.1415926
    return str(degrees)
