from sklearn.preprocessing import MinMaxScaler
import pandas as pd
import numpy as np
from keras.models import load_model

def predict(comstring):

  input_data = pd.read_json(comstring)

  testingDatasetSize = input_data.shape[0]

  #clean data
  input_data.isna().any()

  # scaling testing data
  scaler=MinMaxScaler(feature_range=(0,1))
  google_scaled=scaler.fit_transform(input_data) # size x 12

  # construct test data set
  X_test=[]
  for i in range(60,testingDatasetSize):
    X_test.append(google_scaled[ i-60 : i , 0]) # 500 x 60 x 1
  X_test = np.array(X_test)
  # print("X_test.shape: " , X_test.shape) # 500 x 60

  X_test = np.reshape(X_test, (X_test.shape[0], X_test.shape[1], 1))
  # print(X_test.shape) # 500 x 60 x 1

  new_model = load_model("/container/model.h5")
  predicted_stock_price = new_model.predict(X_test)
  # print(predicted_stock_price.shape) # 500 x 1
  predicted_stock_price = predicted_stock_price.ravel()

  # transform the data back
  highPriceData = input_data.loc[:,'High'].values
  highPriceDataMax = np.amax(highPriceData)
  highPriceDataMin = np.amin(highPriceData)

  lowPriceData = input_data.loc[:,'Low'].values
  lowPriceDataMax = np.amax(lowPriceData)
  lowPriceDataMin = np.amin(lowPriceData)

  openPriceData = input_data.loc[:,'Open'].values
  openPriceDataMax = np.amax(openPriceData)
  openPriceDataMin = np.amin(openPriceData)

  closePriceData = input_data.loc[:,'Close'].values
  closePriceDataMax = np.amax(closePriceData)
  closePriceDataMin = np.amin(closePriceData)

  predicted_stock_price_high = predicted_stock_price*(highPriceDataMax - highPriceDataMin) + highPriceDataMin
  predicted_stock_price_low = predicted_stock_price*(lowPriceDataMax - lowPriceDataMin) + lowPriceDataMin
  predicted_stock_price_open = predicted_stock_price*(openPriceDataMax - openPriceDataMin) + openPriceDataMin
  predicted_stock_price_close = predicted_stock_price*(closePriceDataMax - closePriceDataMin) + closePriceDataMin

  predicted_stock_price = 1/4 * (predicted_stock_price_close + predicted_stock_price_open + predicted_stock_price_high + predicted_stock_price_low)

  return str(predicted_stock_price.tolist()[-10:])