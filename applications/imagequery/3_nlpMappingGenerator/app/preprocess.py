# https://towardsdatascience.com/a-practitioners-guide-to-natural-language-processing-part-i-processing-understanding-text-9f4abfd13e72
from nltk.tokenize import sent_tokenize, word_tokenize
import spacy
import numpy as np
import nltk
import re
import string
nlp = spacy.load('en_core_web_md')
stopword_list = nltk.corpus.stopwords.words('english')
stopword_list.remove('no')
stopword_list.remove('not')

""" input: string; output: string. """
def simple_stemmer(text):
    ps = nltk.stem.porter.PorterStemmer()
    text = ' '.join([ps.stem(word) for word in text.split()])
    return text


""" input: string; output: string. """
def lemmatize_text(text):
    text = nlp(text)
    text = ' '.join([word.lemma_ if word.lemma_ !=
                     '-PRON-' else word.text for word in text])
    return text


""" input: string; output: string. """
def remove_stopwords(text, is_lower_case=False):
    tokens = word_tokenize(text)
    tokens = [token.strip() for token in tokens]
    if is_lower_case:
        filtered_tokens = [
            token for token in tokens if token not in stopword_list]
    else:
        filtered_tokens = [
            token for token in tokens if token.lower() not in stopword_list]
    filtered_text = ' '.join(filtered_tokens)
    return filtered_text


""" input: string; output: string. """
def remove_punctuation(text):
    return (re.sub(r'[!,.:;-](?= |$)', r'', text))


""" input: list of string; output: list of string. """
def normalize_corpus(corpus, remove_punc=True,
                     accented_char_removal=True, text_lower_case=False,
                     text_lemmatization=True, special_char_removal=True,
                     stopword_removal=True, remove_digits=True):

    normalized_corpus = []
    # normalize each document in the corpus
    for doc in corpus:
        # lowercase the text
        if text_lower_case:
            doc = doc.lower()

        # remove extra newlines
        doc = re.sub(r'[\r|\n|\r\n]+', ' ', doc)

        # lemmatize text
        if text_lemmatization:
            doc = lemmatize_text(doc)

        # remove extra whitespace
        doc = re.sub(' +', ' ', doc)

        # remove stopwords
        if stopword_removal:
            doc = remove_stopwords(doc, is_lower_case=text_lower_case)

        # remove punctuation
        if remove_punc:
            doc = remove_punctuation(doc)

        normalized_corpus.append(doc)

    return normalized_corpus


""" input: string; output: list of string. """
def generateCorpus(txt):
    # We are tockenizing sentence instead of words
    return sent_tokenize(txt)


""" input: list of string; output: string. """
def construct_text_from_corpus(corpus):
    text = ""
    for sentence in corpus:
        text += sentence
    return text


""" input: string; output: string. """
def preprocess(text):
    corpus = generateCorpus(text)
    normalized_corpus = normalize_corpus(corpus)
    preprocessed_text = construct_text_from_corpus(normalized_corpus)
    return preprocessed_text


if __name__ == "__main__":
    txt = "A young man plays basketball. A young man runs, jumps. He wears Jordan shoes. It is 3 o'clock now. He is in a seaside park in China. Seaside. Mountain. Yesterday: April 22th Tuesday.Today: April 23th Tuesday."
    print(preprocess(txt))
