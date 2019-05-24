
import scipy.misc
import json

def predict(i):
	image_path = "/container/dataset/" + str(i) + ".jpg"
	with open(image_path, "rb") as imageFile:
		string = base64.encodestring(imageFile.read())
	return string