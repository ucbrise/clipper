
# import scipy.misc

# def predict(i):
# 	image_path = "/container/dataset/" + str(i) + ".jpg"
# 	with open(image_path, "rb") as imageFile:
# 		string = base64.encodestring(imageFile.read())
# 	return string

def predict(input):
	return input.split("*")[0]


