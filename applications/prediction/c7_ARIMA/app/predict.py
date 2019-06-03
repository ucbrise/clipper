#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Apr  8 11:44:55 2019

@author: davidzhou
"""

import pandas as pd
from sklearn.preprocessing import MinMaxScaler
from pyramid.arima import auto_arima
import time

scaler = MinMaxScaler(feature_range=(0, 1))

def predict(comstr):
    start = time.time()
    df = pd.read_json(comstr)
    #setting index as date
    df['Date'] = pd.to_datetime(df.index,format='%Y-%m-%d')
    df.index = df['Date']
    #creating dataframe with date and the target variable
    data = df.sort_index(ascending=True, axis=0)
    train=data[:]
    training=train['Close']
    model = auto_arima(training, start_p=1, start_q=1,max_p=1, max_q=1, m=12,start_P=0, seasonal=True,d=1, D=1, trace=True,error_action='ignore',suppress_warnings=True)
    model.fit(training)
    forecast = model.predict(n_periods=10)
    end = time.time()
    print("ELASPSED TIME", end - start)
    return str(forecast.tolist())
#    forecast = pd.DataFrame(forecast,index = valid.index,columns=['Prediction'])
