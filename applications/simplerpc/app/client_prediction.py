import xmlrpc.client
import pandas as pd
import numpy as np

def run():
    print("\nStart Prediciting: ")

    stock_name = "AAPL"
    print("\nStock name: " + stock_name)

    # CONTAINER 1: stock price retriever
    container1 = xmlrpc.client.ServerProxy('http://localhost:8000')
    stock_data = pd.read_json(container1.Predict(stock_name))
    print("\nStock price data Retrieval FINISHED")
    print("The retrieved data is in shape of ", stock_data.shape)
    print("Here are the first 5 lines of retrieved data:")
    print(stock_data.head())

    # CONTAINER 5: LSTM
    container5 = xmlrpc.client.ServerProxy('http://localhost:13000')
    result_lstm = container5.Predict(stock_data.to_json())
    print("\nPrediction using LSTM FINISHED")
    print("Here is the result:")
    print(result_lstm)

    # CONTAINER 7: ARIMA
    container7 = xmlrpc.client.ServerProxy('http://localhost:15000')
    result_arima = container7.Predict(stock_data.to_json())
    print("\nPrediction using ARIMA FINISHED")
    print("Here is the result:")
    print(result_arima)

    # CONTAINER 8: KNN
    container8 = xmlrpc.client.ServerProxy('http://localhost:16000')
    result_knn = container8.Predict(stock_data.to_json())
    print("\nPrediction using KNN FINISHED")
    print("Here is the result:")
    print(result_knn)

    # CONTAINER 9: Random Forest
    container9 = xmlrpc.client.ServerProxy('http://localhost:17000')
    result_rf = container9.Predict(stock_data.to_json())
    print("\nPrediction using Random Forest FINISHED")
    print("Here is the result:")
    print(result_rf)

    # CONTAINER 10: Regression
    container10 = xmlrpc.client.ServerProxy('http://localhost:18000')
    result_rg = container10.Predict(stock_data.to_json())
    print("\nPrediction using Regrerssion FINISHED")
    print("Here is the result:")
    print(result_rg)

    # CONTAINER 2: Twitter Collector
    container2 = xmlrpc.client.ServerProxy('http://localhost:9001')
    tweet_number = 1000
    twitter_data = container2.Predict(stock_name, tweet_number)
    print("\nTwitter data Retrieval FINISHED")
    print("Successfully retrieved", tweet_number, "number of tweets.")
    print("Here are the first 200 characters:")
    print(twitter_data[:200])

    # CONTAINER 3: Tokenizer
    container3 = xmlrpc.client.ServerProxy('http://localhost:11000')
    tokenized_twitter_data = container3.Predict(twitter_data)
    print("\nTokenization FINISHED")
    print("Generated a list containing ", len(tokenized_twitter_data), " sentences")
    print("The first sentence is :\n", tokenized_twitter_data[0])

    # CONTAINER 4: sentimental Analysis
    container4 = xmlrpc.client.ServerProxy('http://localhost:12000')
    polarity_list = container4.Predict(tokenized_twitter_data)
    print("\nTwitter data Sentiment Analysis FINISHED")
    print("Generated a list containing ", len(polarity_list), " results")
    print("The first result is :\n", polarity_list[0])

    # CONTAINER 11: Weighting Algorithm
    container11 = xmlrpc.client.ServerProxy('http://localhost:19000')
    final_prediction = container11.Predict([result_arima, result_knn, result_lstm, result_rg], result_rf, polarity_list )
    print("\n\nENTIRE PROCESS FINISHED")
    print("HERE IS THE FINAL PREDICTION FOR THE STOCK PRICES OF THE NEXT FEW DAYS:")
    print(final_prediction)

if __name__ == "__main__":
    run()