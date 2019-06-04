# Gensim
# import gensim
# from gensim.utils import simple_preprocess
# spacy for lemmatization
import spacy
import re

# Enable logging for gensim - optional
import logging
logging.basicConfig(format='%(asctime)s : %(levelname)s : %(message)s', level=logging.ERROR)

# NLTK Stop words
import nltk
nltk.download('stopwords')
from nltk.corpus import stopwords
stop_words = stopwords.words('english')
stop_words.extend(['from', 'subject', 're', 'edu', 'use'])
nlp = spacy.load('en', disable=['parser', 'ner'])


# tokenization
# def sent_to_words(sentences):
#     for sentence in sentences:
#         yield (gensim.utils.simple_preprocess(str(sentence), deacc=True))  # deacc=True removes punctuations


def lemmatization(texts, allowed_postags=['NOUN', 'ADJ', 'VERB', 'ADV']):
    """https://spacy.io/api/annotation"""
    texts_out = []
    for sent in texts:
        doc = nlp(" ".join([sent]))
        texts_out.append([token.lemma_ for token in doc if token.pos_ in allowed_postags])
    return texts_out


def text_clean(text):
    data = text.split()
    # Remove new line characters
    data = [re.sub('\s+', ' ', sent) for sent in data]
    # Remove distracting single quotes
    data = [re.sub("\'", "", sent) for sent in data]
    data_words = [y for x in data for y in x.split(' ') ]
    # Remove Stop Words and lemmatizeation
    data_words_nostops = data_words
    data_lemmatized = lemmatization(data_words_nostops, allowed_postags=['NOUN', 'ADJ', 'VERB', 'ADV'])
    final = [word for word in data_lemmatized if word != []]
    data = ["".join(words) for words in final]
    return data


def predict(text_data):
    

    wordsList = text_clean(text_data)

    # do simple analysis
    d = {}
    count = 0
    for word in wordsList:
        if word not in d:
            d[word] = 0
        d[word] += 1
        count += 1
    word_freq = []
    for key, value in d.items():
        word_freq.append((value, key))
    word_freq.sort(reverse=True)

    subject_analysis_result = "After cleaning: \n"
    subject_analysis_result += "Total number of meaningful words: " + str(count) + "\n"
    subject_analysis_result += "Top three frequent words: (count, word)\n"
    for value, freq in word_freq[:3]:
        subject_analysis_result += "The word " + str(freq) + " has appeared for " + str(value) + " times in this text\n"

    return str(subject_analysis_result)

