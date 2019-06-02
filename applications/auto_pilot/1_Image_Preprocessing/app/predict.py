import numpy as np
import cv2
import time

def keras_process_image(img):
	image_x = 40
	image_y = 40
	img = cv2.resize(img, (image_x, image_y))
	img = np.array(img, dtype=np.float32)
	img = np.reshape(img, (-1, image_x, image_y, 1))
	return img

def read_image(i):
	image_path = "/container/dataset/" + i + ".jpg"
	image = cv2.imread(image_path)
	print("original shape", image.shape)
	return image

def predict(info):
	try:
		start = time.time()
		image_index_str = info.split("***")[0]
		image = read_image(image_index_str)
		gray = cv2.resize((cv2.cvtColor(image, cv2.COLOR_RGB2HSV))[:, :, 1], (40, 40))
		print("resized shape", gray.shape)
		end = time.time()
		print("ELASPSED TIME", end - start)
		return info
	except Exception as exc:
		print('Generated an exception: %s' % (exc))

