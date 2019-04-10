#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Mar 20 12:10:10 2019

@author: davidzhou
"""

import xmlrpc.client
from xmlrpc.server import SimpleXMLRPCServer

# Gensim
import gensim
from gensim.utils import simple_preprocess
# spacy for lemmatization
import spacy
import re
import numpy as np
from pprint import pprint

# Enable logging for gensim - optional
import logging
logging.basicConfig(format='%(asctime)s : %(levelname)s : %(message)s', level=logging.ERROR)

# NLTK Stop words
from nltk.corpus import stopwords


stop_words = stopwords.words('english')
stop_words.extend(['from', 'subject', 're', 'edu', 'use'])
# python3 -m spacy download en
nlp = spacy.load('en', disable=['parser', 'ner'])

#tokenization
def sent_to_words(sentences):
    for sentence in sentences:
        yield(gensim.utils.simple_preprocess(str(sentence), deacc=True))  # deacc=True removes punctuations
        
# Define functions for stopwords and lemmatization
def remove_stopwords(texts):
    return [[word for word in simple_preprocess(str(doc)) if word not in stop_words] for doc in texts]

def lemmatization(texts, allowed_postags=['NOUN', 'ADJ', 'VERB', 'ADV']):
    """https://spacy.io/api/annotation"""
    texts_out = []
    for sent in texts:
        doc = nlp(" ".join(sent)) 
        texts_out.append([token.lemma_ for token in doc if token.pos_ in allowed_postags])
    return texts_out

def text_clean(text):
    data=text.split()
    # Remove new line characters
    data = [re.sub('\s+', ' ', sent) for sent in data]

    # Remove distracting single quotes
    data = [re.sub("\'", "", sent) for sent in data]
    data_words = list(sent_to_words(data))
    # Remove Stop Words and lemmatizeation
    data_words_nostops = remove_stopwords(data_words)
    data_lemmatized = lemmatization(data_words_nostops, allowed_postags=['NOUN', 'ADJ', 'VERB', 'ADV'])
    final=[word for word in data_lemmatized if word!=[]]
    data=["".join(words) for words in final]
    return data

server = SimpleXMLRPCServer(("0.0.0.0", 11003))

print("Listening on port 11003...")

server.register_function(text_clean, "text_clean")

server.serve_forever()
    




#text=[['health','drug','play']]
#id2word = corpora.Dictionary(text)
#corpus=[id2word.doc2bow(text[0])]
#
#print(corpus)
#vector=myLDA[corpus]
#pprint(vector)
#pprint(myLDA.print_topics(num_words=20))