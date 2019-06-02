from sklearn.preprocessing import MinMaxScaler
import pandas as pd
import numpy as np
import quandl
import datetime
from keras.models import Sequential
from keras.layers import Dense
from keras.layers import LSTM
from keras.layers import Dropout
from pandas.compat import StringIO

def trainModel(stock_code):
  start = datetime.datetime(2016, 1, 1)
  end = datetime.date.today()
  stock_price_dataframe = quandl.get("WIKI/" + stock_code, start_date=start, end_date=end)
  # print(stock_price_dataframe.index)
  stock_price_dataframe.reset_index(inplace=True)
  stock_price_dataframe.reset_index(inplace=False)
  # print(stock_price_dataframe)

  stock_price_string = stock_price_dataframe.to_string(index=False)
  print(stock_price_string)
  df = pd.read_csv(StringIO(stock_price_string), sep=r'\s+');
  df = df.iloc[:, 0:11]
  print(df);

  training_set = stock_price_dataframe
  training_set_size = training_set.shape[0]

  # # Data cleaning
  # training_set.isna().any()

  # Feature Scaling Normalization
  # scaler = MinMaxScaler(feature_range=(0, 1))
  # training_set_scaled = scaler.fit_transform(training_set)
  # print("training_set_scaled.shape: " , training_set_scaled.shape)

  # # Creating a data structure with 60 timesteps and 1 output
  # X_train = []
  # y_train = []
  # for i in range(60, training_set_size):
  #     X_train.append(training_set_scaled[i-60 : i, 0])
  #     y_train.append(training_set_scaled[i, 0])
  # X_train, y_train = np.array(X_train), np.array(y_train)

  # print("X_train.shape: " , X_train.shape) # 500 x 60
  # print("y_train.shape: " , y_train.shape) # 500 x 1

  # # Reshaping
  # X_train = np.reshape(X_train, (X_train.shape[0], X_train.shape[1], 1)) 
  # print("After reshape, X_train.shape: " , X_train.shape) # 500 x 60 x 1

  # #Building the RNN LSTM model
  # # Initialising the RNN
  # regressor = Sequential()
  # # Adding the first LSTM layer and some Dropout regularisation
  # regressor.add(LSTM(units = 50, return_sequences = True, input_shape = (X_train.shape[1], 1)))
  # regressor.add(Dropout(0.2))

  # # Adding a second LSTM layer and some Dropout regularisation
  # regressor.add(LSTM(units = 50, return_sequences = True))
  # regressor.add(Dropout(0.2))

  # # Adding a third LSTM layer and some Dropout regularisation
  # regressor.add(LSTM(units = 50, return_sequences = True))
  # regressor.add(Dropout(0.2))

  # # Adding a fourth LSTM layer and some Dropout regularisation
  # regressor.add(LSTM(units = 50))
  # regressor.add(Dropout(0.2))

  # # Adding the output layer
  # regressor.add(Dense(units = 1))
  # # Compiling the RNN
  # regressor.compile(optimizer = 'adam', loss = 'mean_squared_error')

  # # Fitting the RNN to the Training set
  # regressor.fit(X_train, y_train, epochs = 10, batch_size = 32)

  # regressor.save("model.h5")
  # print("Trainig ends, parameters saved in model.h5")

if __name__ == "__main__":
  trainModel('AAPL')