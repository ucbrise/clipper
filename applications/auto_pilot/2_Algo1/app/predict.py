import tensorflow as tf
import model
import scipy.misc
import os

sess = tf.InteractiveSession()
saver = tf.train.Saver()
saver.restore(sess, "/container/model.ckpt")

def read_image(i):
	image_path = "container/dataset/" + i + ".jpg"
	print(image_path)
	image = scipy.misc.imread(image_path, mode="RGB").tolist()
	print("image shape is ", image.shape)
	return image

def predict(i):
	try:
		myCmd = 'ls container'
		os.system(myCmd)
		image = read_image(i)
		image = scipy.misc.imresize(image[-150:], [66, 200]) / 255.0
		print("resized image shape is ", image.shape)
		degrees = model.y.eval(feed_dict={model.x: [image], model.keep_prob: 1.0})[0][0] * 180.0 / 3.1415926
		print(degrees)
		return str(degrees)
	except Exception as exc:
		print('%s generated an exception: %s' % (str(inputt), exc))
