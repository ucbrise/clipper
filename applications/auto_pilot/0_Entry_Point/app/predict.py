# A dummpy container that just returns whatever is feeded into it

import time

def predict(input):
	start = time.time()
	end = time.time()
	print("ELASPSED TIME", end - start)
	return input


