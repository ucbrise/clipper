import spacy
from preprocess import preprocess
from timeit import default_timer as timer

nlp = spacy.load("en_core_web_md")

def predict(input_str):
    start = timer()

    c1_output, c2_output = str(input_str).split("|")
    reconstructed = str(c1_output) + str(c2_output)
    print(reconstructed)
    preprocessed = preprocess(reconstructed)
    print(preprocessed)

    doc = nlp(preprocessed)
    noun_list = [chunk.text for chunk in doc.noun_chunks]
    verb_list = [token.lemma_ for token in doc if token.pos_ == "VERB"]

    noun_str = ", ".join(noun_list)
    verb_str = ", ".join(verb_list)

    end = timer()
    time_elapsed = end - start
    print("The nlp analysis takes " + str(time_elapsed) + " seconds")
    
    print(noun_str + "-" + verb_str)
    return noun_str + "-" + verb_str

if __name__ == '__main__':
    predict("please call Stella ask her to bring. |a small propeller plane sitting on top of a field .")
    