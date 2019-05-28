import spacy,en_core_web_sm

def predict(text_data):

    nlp = en_core_web_sm.load()

    doc = nlp(text_data)

    result = ""

    print(doc.sents)

    for sent in doc.sents:
        result += "|||" + str(sent)

    print(len(result.split("|||")))

    return result