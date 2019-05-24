
import keras.models as models

from keras.models import Sequential, Model
from keras.optimizers import SGD, Adam, RMSprop

import cv2
import numpy as np
import json

# load the model:
model = Sequential()
with open('/container/autopilot_basic_model.json') as model_file:
    model = models.model_from_json(model_file.read())

# load weights
model.load_weights("/container/model_basic_weight.hdf5")

adam = Adam(lr=0.0001)

model.compile(loss='mse', optimizer=adam, metrics=['mse','accuracy'])

def predict(image_str):
	fh = open("temp.jpg", "wb")
    fh.write(base64.decodebytes(image_str))
    fh.close()
    image = scipy.misc.imread("temp.jpg", mode="RGB").tolist()
    image = np.asarray(image.astype(np.float32))
    resized = cv2.resize(image, (128,128))
    preds = model.predict(resized.reshape(1,3,128,128))
    steer_preds = (preds.reshape([-1])+1)/2.
    return str(steer_preds[0])

