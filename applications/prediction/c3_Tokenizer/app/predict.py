import spacy,en_core_web_sm

def predict(text_data):

    nlp = en_core_web_sm.load()

    doc = nlp(text_data)

    result = "result"

    print(doc.sents)

    for sent in doc.sents:
        result= result + "-" + str(sent)

    print(len(result.split("-")[1:]))

    return result