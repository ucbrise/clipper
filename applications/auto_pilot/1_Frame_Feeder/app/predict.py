
import scipy.misc
import json

def predict(i):
  full_image = scipy.misc.imread("/container/dataset/" + str(i) + ".jpg", mode="RGB")
  return json.dumps(full_image.tolist())