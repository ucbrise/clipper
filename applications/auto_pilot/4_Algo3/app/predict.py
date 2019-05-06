import json
import numpy as np
import cv2
from keras.models import load_model

model = load_model('/container/Autopilot_V2.h5')

def keras_predict(model, image):
    processed = keras_process_image(image)
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

def predict(image_str):
    image = np.asarray(json.loads(image_str)).astype(np.float32)
    gray = cv2.resize((cv2.cvtColor(image, cv2.COLOR_RGB2HSV))[:, :, 1], (100, 100))
    steering_angle = keras_predict(model, gray)
    return str(steering_angle)
