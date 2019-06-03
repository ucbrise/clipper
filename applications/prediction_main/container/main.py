import pandas as pd
import numpy as np
import time


import sys
sys.path.append("/container")

from multiprocessing import Pool 

# c7 is discarded in this file, import error

import c1_Stock_Price_Retriever.app.predict as c1
import c2_Twitter_Collector.app.predict as c2 
import c3_Tokenizer.app.predict as c3
import c4_Sentiment_Analysis.app.predict as c4 
import c5_LSTM_Predictor.app.predict as c5
import c6_Mem.app.predict as c6 
import c7_ARIMA.app.predict as c7 
import c8_KNN.app.predict as c8 
import c9_RandomForest.app.predict as c9 
import c10_Regression.app.predict as c10 
import c11_Conclusion.app.predict as c11 
print("Modules successfully loaded!")

def run_lstm(stock_data):
  result_lstm = c5.predict(stock_data.to_json())
  print("Prediction using LSTM FINISHED")
  print("Here is the result:")
  print(result_lstm)
  print("")
  return str(result_lstm)

def run_knn(stock_data):
  result_knn = c8.predict(stock_data.to_json())
  print("Prediction using KNN FINISHED")
  print("Here is the result:")
  print(result_knn)
  print("")
  return result_knn

def run_random_forest(stock_data):
  result_rf = c9.predict(stock_data.to_json())
  print("Prediction using Random Forest FINISHED")
  print("Here is the result:")
  print(result_rf)
  print("")
  return result_rf

def run_arima(stock_data):
  result_rf = c7.predict(stock_data.to_json())
  print("Prediction using ARIMA FINISHED")
  print("Here is the result:")
  print(result_rf)
  print("")
  return result_rf

def run_regression(stock_data):
  result_rg = c10.predict(stock_data.to_json())
  print("Prediction using Regrerssion FINISHED")
  print("Here is the result:")
  print(result_rg)
  print("")
  return result_rg

def run():

    start = time.time()

    print("\nStart Prediciting: ")

    # CONTAINER 1: stock price retriever
    stock_data = pd.read_json(c1.predict("AAPL:2018:1:1"))
    print("\nStock price data Retrieval FINISHED")
    print("The retrieved data is in shape of ", stock_data.shape)
    print("Here are the first 5 lines of retrieved data:")
    print(stock_data.head())
    print("")

    returned_result_list = []
    p = Pool(5)
    returned_result_list.append(p.apply_async(run_lstm, args=(stock_data,))) 
    returned_result_list.append(p.apply_async(run_knn, args=(stock_data,)))
    returned_result_list.append(p.apply_async(run_random_forest, args=(stock_data,)))
    returned_result_list.append(p.apply_async(run_regression, args=(stock_data,)))
    returned_result_list.append(p.apply_async(run_arima, args=(stock_data,)))
    p.close()
    p.join() # p.join()方法会等待所有子进程执行完毕

    # CONTAINER 2: Twitter Collector
    tweet_number = 1000
    twitter_data = c2.predict("Apple")
    print("Twitter data Retrieval FINISHED")
    print("Successfully retrieved", tweet_number, "number of tweets.")
    print("Here are the first 200 characters:")
    print(twitter_data[:200])
    print("")

    # CONTAINER 3: Tokenizer
    tokenized_twitter_data = c3.predict(twitter_data)
    print("Tokenization FINISHED")
    print("Generated a list containing ", len(tokenized_twitter_data), " sentences")
    print("The first five sentences are :\n", tokenized_twitter_data[:5])
    print("")

    # CONTAINER 4: sentimental Analysis
    polarity_list = c4.predict(tokenized_twitter_data)
    print("Twitter data Sentiment Analysis FINISHED")
    print("Generated a list containing ", len(polarity_list), " results")
    print("The first five results are :\n", polarity_list[:5])
    print("")

    # CONTAINER 11: Weighting Algorithm
    final_prediction = c11.predict(str(returned_result_list))
    print("\n\nEntire Process FINISHED")

    print("Total Time:", time.time()-start)


if __name__ == "__main__":
    run()