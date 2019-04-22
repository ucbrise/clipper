
import scipy.misc
import json

def predict(i):
  full_image = scipy.misc.imread("/container/dataset/" + str(i) + ".jpg", mode="RGB")
  image = scipy.misc.imresize(full_image[-150:], [66, 200]) / 255.0
  return json.dumps(image.tolist())