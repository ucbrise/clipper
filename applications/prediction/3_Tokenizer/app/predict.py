import spacy,en_core_web_sm

def predict(text_data):

    nlp = en_core_web_sm.load()

    doc = nlp(text_data)

    result = []

    for sent in doc.sents:
        result.append(str(sent))

    return result