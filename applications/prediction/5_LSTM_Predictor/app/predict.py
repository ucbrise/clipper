from sklearn.preprocessing import MinMaxScaler
import pandas as pd
import numpy as np
import quandl
import datetime
from keras.models import Sequential
from keras.layers import Dense
from keras.layers import LSTM
from keras.layers import Dropout
from keras.models import load_model

def predict(stock_code):
  # Loading data
  start = datetime.datetime(2016, 1, 1)
  end = datetime.date.today()
  input_data = quandl.get("WIKI/" + stock_code, start_date=start, end_date=end)
  input_data_string = input_data.to_string(index=False)
  
  print(input_data_string)

  testingDatasetSize = 560
  input_data = input_data.head(560)
  print(type(input_data))

  # #clean data
  # input_data.isna().any()

  # # scaling testing data
  # scaler=MinMaxScaler(feature_range=(0,1)) 
  # google_scaled=scaler.fit_transform(input_data) # size x 12

  # # construct test data set
  # X_test=[]
  # for i in range(60,testingDatasetSize):
  #   X_test.append(google_scaled[ i-60 : i , 0]) # 500 x 60 x 1
  # X_test = np.array(X_test)
  # # print("X_test.shape: " , X_test.shape) # 500 x 60

  # X_test = np.reshape(X_test, (X_test.shape[0], X_test.shape[1], 1))
  # # print(X_test.shape) # 500 x 60 x 1

  # new_model = load_model('./prediction/5_LSTM_Predictor/app/model.h5')
  # predicted_stock_price = new_model.predict(X_test)
  # # print(predicted_stock_price.shape) # 500 x 1
  # predicted_stock_price = predicted_stock_price.ravel()

  # # transform the data back
  # highPriceData = input_data.loc[:,'High'].values
  # highPriceDataMax = np.amax(highPriceData)
  # highPriceDataMin = np.amin(highPriceData)

  # lowPriceData = input_data.loc[:,'Low'].values
  # lowPriceDataMax = np.amax(lowPriceData)
  # lowPriceDataMin = np.amin(lowPriceData)

  # openPriceData = input_data.loc[:,'Open'].values
  # openPriceDataMax = np.amax(openPriceData)
  # openPriceDataMin = np.amin(openPriceData)

  # closePriceData = input_data.loc[:,'Close'].values
  # closePriceDataMax = np.amax(closePriceData)
  # closePriceDataMin = np.amin(closePriceData)

  # predicted_stock_price_high = predicted_stock_price*(highPriceDataMax - highPriceDataMin) + highPriceDataMin
  # predicted_stock_price_low = predicted_stock_price*(lowPriceDataMax - lowPriceDataMin) + lowPriceDataMin
  # predicted_stock_price_open = predicted_stock_price*(openPriceDataMax - openPriceDataMin) + openPriceDataMin
  # predicted_stock_price_close = predicted_stock_price*(closePriceDataMax - closePriceDataMin) + closePriceDataMin

  # predicted_stock_price = 1/4 * (predicted_stock_price_close + predicted_stock_price_open + predicted_stock_price_high + predicted_stock_price_low)

  print(predicted_stock_price)



if __name__ == "__main__":
    predict('GOOGL')
