import spacy,en_core_web_sm
import time

def predict(text_data):

	start = time.time()

    nlp = en_core_web_sm.load()

    doc = nlp(text_data)

    result = ""

    print(doc.sents)

    for sent in doc.sents:
        result += "|||" + str(sent)

    print(len(result.split("|||")))

    end = time.time()
    
    print("ELASPSED TIME", end - start)

    return result