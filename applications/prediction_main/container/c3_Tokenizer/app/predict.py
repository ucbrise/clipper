import spacy,en_core_web_sm
import time

def predict(text_data):

	start = time.time()

	nlp = en_core_web_sm.load()

	doc = nlp(text_data)

	result = ""

	for sent in doc.sents:
		result += "|||" + str(sent) 

	end = time.time()
	
	print("c3 ELASPSED TIME", end - start)

	return result