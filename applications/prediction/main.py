import pandas as pd
import numpy as np

import sys
sys.path.append("/container")

import c1_Stock_Price_Retriever.app.predict as c1
import c2_Twitter_Collector.app.predict as c2 
import c3_Tokenizer.app.predict as c3
import c4_Sentiment_Analysis.app.predict as c4 
import c5_LSTM_Predictor.app.predict as c5
import c6_Mem.app.predict as c6 
# import c7_ARIMA.app.predict as c7 
import c8_KNN.app.predict as c8 
import c9_RandomForest.app.predict as c9 
import c10_Regression.app.predict as c10 
import c11_Conclusion.app.predict as c11 
print("Modules successfully loaded!")

def run():
    print("\nStart Prediciting: ")

    stock_name = "AAPL"
    print("\nStock name: " + stock_name)

    # CONTAINER 1: stock price retriever
    stock_data = pd.read_json(c1.predict(stock_name))
    print("\nStock price data Retrieval FINISHED")
    print("The retrieved data is in shape of ", stock_data.shape)
    print("Here are the first 5 lines of retrieved data:")
    print(stock_data.head())

    # CONTAINER 5: LSTM
    result_lstm = c5.predict(stock_data.to_json())
    print("\nPrediction using LSTM FINISHED")
    print("Here is the result:")
    print(result_lstm)

    # CONTAINER 7: ARIMA
    # result_arima = c7.predict(stock_data.to_json())
    # print("\nPrediction using ARIMA FINISHED")
    # print("Here is the result:")
    # print(result_arima)

    # CONTAINER 8: KNN
    result_knn = c8.predict(stock_data.to_json())
    print("\nPrediction using KNN FINISHED")
    print("Here is the result:")
    print(result_knn)

    # CONTAINER 9: Random Forest
    result_rf = c9.predict(stock_data.to_json())
    print("\nPrediction using Random Forest FINISHED")
    print("Here is the result:")
    print(result_rf)

    # CONTAINER 10: Regression
    result_rg = c10.predict(stock_data.to_json())
    print("\nPrediction using Regrerssion FINISHED")
    print("Here is the result:")
    print(result_rg)

    # CONTAINER 2: Twitter Collector
    tweet_number = 1000
    twitter_data = c2.predict("placeholder")
    print("\nTwitter data Retrieval FINISHED")
    print("Successfully retrieved", tweet_number, "number of tweets.")
    print("Here are the first 200 characters:")
    print(twitter_data[:200])

    # CONTAINER 3: Tokenizer
    tokenized_twitter_data = c3.predict(twitter_data)
    print("\nTokenization FINISHED")
    print("Generated a list containing ", len(tokenized_twitter_data), " sentences")
    print("The first sentence is :\n", tokenized_twitter_data[0])

    # CONTAINER 4: sentimental Analysis
    print("Start c4:")
    polarity_list = c4.predict(tokenized_twitter_data)
    print("\nTwitter data Sentiment Analysis FINISHED")
    print("Generated a list containing ", len(polarity_list), " results")
    print("The first result is :\n", polarity_list[0])

    # CONTAINER 11: Weighting Algorithm
    final_prediction = c11.predict([result_knn, result_lstm, result_rg], result_rf, polarity_list )
    print("\n\nENTIRE PROCESS FINISHED")
    print("HERE IS THE FINAL PREDICTION FOR THE STOCK PRICES OF THE NEXT FEW DAYS:")
    print(final_prediction)

if __name__ == "__main__":
    run()