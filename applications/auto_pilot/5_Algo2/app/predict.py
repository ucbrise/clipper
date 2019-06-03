import numpy as np
import cv2
import time
import tensorflow as tf
from keras.models import load_model

def keras_predict(model, image):
	processed = keras_process_image(image)
	print("processed shape", processed.shape)
	steering_angle = float(model.predict(processed, batch_size=1))
	steering_angle = steering_angle * 60
	return steering_angle

def keras_process_image(img):
	image_x = 100
	image_y = 100
	img = cv2.resize(img, (image_x, image_y))
	img = np.array(img, dtype=np.float32)
	img = np.reshape(img, (-1, image_x, image_y, 1))
	return img

def read_image(i):
	image_path = "/container/dataset/" + i + ".jpg"
	image = cv2.imread(image_path)
	print("original shape", image.shape)
	return image

global graph, model

graph = tf.get_default_graph()
	
model = load_model('/container/Autopilot_V2.h5')

def predict(info):
	try:
		start = time.time()
		image_index_str = info.split("***")[2]
		image = read_image(image_index_str)
		gray = cv2.resize((cv2.cvtColor(image, cv2.COLOR_RGB2HSV))[:, :, 1], (100, 100))
		print("resized shape", image.shape)
		with graph.as_default():
			steering_angle = keras_predict(model, gray)
		end = time.time()
		print("ELASPSED TIME", end - start)
		return str(steering_angle) + "***" + info
	except Exception as exc:
		print('Generated an exception: %s' % (exc))
