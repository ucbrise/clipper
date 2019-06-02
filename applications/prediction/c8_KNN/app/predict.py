#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Apr  8 11:00:32 2019

@author: davidzhou
"""

import time
import pandas as pd
from sklearn import neighbors
from sklearn.model_selection import GridSearchCV
from sklearn.preprocessing import MinMaxScaler
from fastai.tabular import add_datepart

scaler = MinMaxScaler(feature_range=(0, 1))

def predict(comstring):

    start = time.time()
    df = pd.read_json(comstring)

    df['Date'] = pd.to_datetime(df.index,format='%Y-%m-%d')
    df.index = df['Date']

    data = df.sort_index(ascending=True, axis=0)
   
    new_data = pd.DataFrame(index=range(0,len(df)),columns=['Date', 'Close'])
    
    for i in range(0,len(data)):
         new_data['Date'][i] = data['Date'][i]
         new_data['Close'][i] = data['Close'][i]

    #create features
    add_datepart(new_data, 'Date')

    new_data.drop('Elapsed', axis=1, inplace=True)  #elapsed will be the time stamp
    
    #split into train and validation
    train = new_data[:-10]
    valid = new_data[-10:]
    x_train = train.drop('Close', axis=1)
    y_train = train['Close']
    
    x_valid = valid.drop('Close', axis=1)
#   y_valid = valid['Close']
    
    #implement KNN
    x_train_scaled = scaler.fit_transform(x_train)
    x_train = pd.DataFrame(x_train_scaled)
    x_valid_scaled = scaler.fit_transform(x_valid)
    x_valid = pd.DataFrame(x_valid_scaled)
    #using gridsearch to find the best parameter
    params = {'n_neighbors':[2,3,4,5,6,7,8,9]}
    knn = neighbors.KNeighborsRegressor()
    model = GridSearchCV(knn, params, cv=5)
    
    #fit the model and make predictions
    model.fit(x_train,y_train)
    preds = [x.item() for x in model.predict(x_valid).tolist()]

    end = time.time()
    print("ELASPSED TIME", end - start)
    
    return str(preds)




