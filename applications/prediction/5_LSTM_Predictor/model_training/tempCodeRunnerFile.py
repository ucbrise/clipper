# Creating a data structure with 60 timesteps and 1 output
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