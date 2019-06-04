import spacy,en_core_web_sm

nlp = en_core_web_sm.load()

def predict(text_data):

    doc = nlp(text_data)

    result = ""

    for sent in doc.sents:
        result = result + str(sent) + ","


    return result
