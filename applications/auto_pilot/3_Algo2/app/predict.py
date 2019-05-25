import numpy as np
import cv2
from keras.models import load_model

model = load_model('/container/Autopilot.h5')

def keras_predict(model, image):
    processed = keras_process_image(image)
    steering_angle = float(model.predict(processed, batch_size=1))
    steering_angle = steering_angle * 100
    return steering_angle

def keras_process_image(img):
    image_x = 40
    image_y = 40
    img = cv2.resize(img, (image_x, image_y))
    img = np.array(img, dtype=np.float32)
    img = np.reshape(img, (-1, image_x, image_y, 1))
    return img

def read_image(i):
    image_path = "/container/dataset/" + i + ".jpg"
    image = scipy.misc.imread(image_path, mode="RGB").tolist()
    return image

def predict(i):
    image = read_image(i)
    image = np.asarray(image.astype(np.float32))
    gray = cv2.resize((cv2.cvtColor(image, cv2.COLOR_RGB2HSV))[:, :, 1], (40, 40))
    steering_angle = keras_predict(model, gray)
    return str(steering_angle)
