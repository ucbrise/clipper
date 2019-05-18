#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Apr  8 11:00:32 2019

@author: davidzhou
"""

import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
    
def predict(comstring):
    data = pd.read_json(comstring)
    # feature extraction
    # Features construction 
    data['Open-Close'] = (data.Open - data.Close)/data.Open
    data['High-Low'] = (data.High - data.Low)/data.Low
    data['percent_change'] = data['Adj. Close'].pct_change()
    data['std_5'] = data['percent_change'].rolling(5).std()
    data['ret_5'] = data['percent_change'].rolling(5).mean()
    data.dropna(inplace=True)
    
    # X is the input variable
    X = data[['Open-Close', 'High-Low', 'std_5', 'ret_5']]
    
    # Y is the target or output variable
    y = np.where(data['Adj. Close'].shift(-1) > data['Adj. Close'], 1, -1)
    # Total dataset length
    dataset_length = data.shape[0]
    
    # Training dataset length
    split = int(dataset_length * 0.75)
    split=-9
    # Splitiing the X and y into train and test datasets
    X_train, X_test = X[:split], X[split:]
    y_train = y[:split]
    
    # Print the size of the train and test dataset
#    print(X_train.shape, X_test.shape)
#    print(y_train.shape, y_test.shape)
    
    clf = RandomForestClassifier(random_state=5)
    # Create the model on train dataset
    model = clf.fit(X_train, y_train)
    preds = model.predict(X_test).tolist()

    return str(preds)

    
    
    
    
    
    
    