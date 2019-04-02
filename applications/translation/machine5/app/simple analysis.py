#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Mar 20 13:29:29 2019

@author: davidzhou
"""
#rpc package
import xmlrpc.client
from xmlrpc.server import SimpleXMLRPCServer

#text is a list of words
def simple_analysis(texts):
    #connect to tokenizor
    #address need to be adjusted
    tokenizor = xmlrpc.client.ServerProxy('http://0.0.0.0:11003')
    wordsList=tokenizor.text_clean(texts)
    
    #do simple analysis
    d={}
    count=0
    for word in wordsList:
        if word not in d:
            d[word]=0
        d[word]+=1
        count+=1
    word_freq = []
    for key, value in d.items():
        word_freq.append((value, key))
    word_freq.sort(reverse=True)
    print("Total number of words: "+str(count)+"\n")
    print("Top three frequent words: (count, word)")
    print(word_freq[:3])
    print("\n")
    return 0

server = SimpleXMLRPCServer(("0.0.0.0", 11004))

print("Listening on port 11002...")

server.register_function(simple_analysis, "simple_analysis")

server.serve_forever()
    
        