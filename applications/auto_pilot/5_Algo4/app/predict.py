
import keras.models as models

from keras.models import Sequential, Model
from keras.optimizers import SGD, Adam, RMSprop

import cv2
import numpy as np
import json
import base64

# load the model:
model = Sequential()
with open('/container/autopilot_basic_model.json') as model_file:
    model = models.model_from_json(model_file.read())

# load weights
model.load_weights("/container/model_basic_weight.hdf5")

adam = Adam(lr=0.0001)

model.compile(loss='mse', optimizer=adam, metrics=['mse','accuracy'])

def read_image(i):
    image_path = "/container/dataset/" + i + ".jpg"
    image = scipy.misc.imread(image_path, mode="RGB").tolist()
    return image

def predict(image_str):
    image = read_image(i)
    image = np.asarray(image.astype(np.float32))
    resized = cv2.resize(image, (128,128))
    preds = model.predict(resized.reshape(1,3,128,128))
    steer_preds = (preds.reshape([-1])+1)/2.
    return str(steer_preds[0])

