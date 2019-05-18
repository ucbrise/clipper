#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Apr  7 22:49:46 2019

@author: davidzhou
"""

import pandas as pd
    
def predict(compstring):
    df = pd.read_json(compstring)
    #print the head
    df.head()

    #setting index as date
    df['Date'] = pd.to_datetime(df.index,format='%Y-%m-%d')
    df.index = df['Date']
    
    #creating dataframe with date and the target variable
    data = df.sort_index(ascending=True, axis=0)
   
    new_data = pd.DataFrame(index=range(0,len(df)),columns=['Date', 'Close'])
    
    for i in range(0,len(data)):
         new_data['Date'][i] = data['Date'][i]
         new_data['Close'][i] = data['Close'][i]
         
    #create features
    from fastai.tabular import  add_datepart
    add_datepart(new_data, 'Date')
    
    new_data.drop('Elapsed', axis=1, inplace=True)  #elapsed will be the time stamp
    
    #split into train and validation
    train = new_data[:-10]
    valid = new_data[-10:]
    x_train = train.drop('Close', axis=1)
    
    y_train = train['Close']
    
    x_valid = valid.drop('Close', axis=1)
#   y_valid = valid['Close']
    
    #implement linear regression
    from sklearn.linear_model import LinearRegression
    model = LinearRegression()
    model.fit(x_train,y_train)
    
    #make predictions and find the rmse
    preds = model.predict(x_valid).tolist()
#   print(preds)
#   rms=np.sqrt(np.mean(np.power((np.array(y_valid)-np.array(preds)),2)))

    return str(preds)
    



