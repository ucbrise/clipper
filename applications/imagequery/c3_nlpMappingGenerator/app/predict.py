# Ref1: spacy official documenation for tagging, noun_chunk 
# Ref2: textacy official documentation for matching pattern
import spacy
import textacy
import textacy.Doc
from c3_nlpMappingGenerator.app.preprocess import preprocess
from timeit import default_timer as timer

# def print_document(doc):
#   import pprint
#   pp = pprint.PrettyPrinter()
#   print('\n')
#   for token in doc:
#       pp.pprint((token.text, token.lemma_, token.pos_,
#                   token.tag_, token.dep_, token.is_stop))


# def print_verb_tagging(doc):
#     print("\nTokens with 'VERB' as POS tag: ")
#     for token in doc:
#         if (token.pos_ == 'VERB'):
#             print((token.text, token.pos_))


def get_time_list(doc):
    time_list = []
    for time in doc.ents:
        if time.label_ == 'TIME' or time.label_ == 'DATE':
            time_list.append(time.text)
    return time_list


def get_noun_chunk_list(doc):
    noun_chunk_list = []
    for chunk in doc.noun_chunks:
        noun_chunk_list.append(chunk.text)
    return noun_chunk_list


def get_verb_phrase_list(txt):
    """
    ? : 0 or 1
    + : >= 1
    * : >= 0
    """
    # Though you can create your own patter, they easily overshoot
    pattern = r'<VERB>*<ADV>*<PART>*<VERB>+<PART>*'
    doc = textacy.Doc(txt, lang='en_core_web_md')
    lists = textacy.extract.pos_regex_matches(doc, pattern)
    verb_phrases = []
    for list in lists:
        verb_phrases.append(list.text)
    return verb_phrases


def generate_mapping(txt):
    nlp = spacy.load('en_core_web_md')
    txt = preprocess(txt)
    document = nlp(txt)

    noun_chunk_list = get_noun_chunk_list(document)
    verb_phrase_list = get_verb_phrase_list(txt)
    time_list = get_time_list(document)

    subject = noun_chunk_list[0] if len(noun_chunk_list) > 0 else "<No subject>"
    verb_phrase = verb_phrase_list[0] if len(verb_phrase_list) > 0 else "<No predicate>"
    time = time_list[0] if len(time_list) > 0 else "<No time>"
    mapping_string = subject + "-" + verb_phrase + "-" + time 
    """ Format: <subject>-<verb>-<time> """
    return mapping_string


def testing():
    txt1 = "A man is happily playing basketball at 3 o'clock."
    txt2 = "A boy is crying silently at night."
    txt3 = "An athelete is running very fast. It was April 1996"
    txt4 = "A woman is running quickly in the morning."

    txt_list = [txt1, txt2, txt3, txt4]
    for index, txt in enumerate(txt_list, start=1):
        """ the index here should is necessary!!! """
        print(generate_mapping(txt))
        print(generate_mapping(txt).split('-'))

def predict(txt):
    start = timer()
    generated_mapping = generate_mapping(txt)
    end = timer()
    time_elapsed = end - start
    return generated_mapping, time_elapsed

# if __name__ == "__main__":
#     txt = "A man is happily playing basketball at 3 o'clock."
#     print(predict(txt)) # man-happily play-3 o'clock
