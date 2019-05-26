import numpy as np
import cv2
from keras.models import load_model

model = load_model('Autopilot.h5')

def keras_predict(model, image):
    processed = keras_process_image(image)
    print("processed shape", processed.shape)
    steering_angle = float(model.predict(processed, batch_size=1))
    steering_angle = steering_angle * 100
    print("result", steering_angle)
    return steering_angle

def keras_process_image(img):
    image_x = 40
    image_y = 40
    img = cv2.resize(img, (image_x, image_y))
    img = np.array(img, dtype=np.float32)
    img = np.reshape(img, (-1, image_x, image_y, 1))
    return img

def read_image(i):
    image_path = i + ".jpg"
    image = cv2.imread(image_path)
    print("original shape", image.shape)
    return image

def predict(i):
    try:
        image = read_image(i)
        gray = cv2.resize((cv2.cvtColor(image, cv2.COLOR_RGB2HSV))[:, :, 1], (40, 40))
        print("resized shape", gray.shape)
        steering_angle = keras_predict(model, gray)
        return str(steering_angle)
    except Exception as exc:
        print('%s generated an exception: %s' % (str(inputt), exc))

predict("1")