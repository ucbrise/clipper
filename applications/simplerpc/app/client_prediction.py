import xmlrpc.client
import numpy as np
from scipy.io import wavfile

def run():

    stock_name = "AAPL"
    
    container1 = xmlrpc.client.ServerProxy('http://0.0.0.0:8000')
    stock_data = container1.Predict(stock_name)
    print("\nData Retrieval FINISHED")

    container2 = xmlrpc.client.ServerProxy('http://0.0.0.0:9000')
    twitter_data = container2.Predict(stock_name)

    container3 = xmlrpc.client.ServerProxy('http://0.0.0.0:11000')
    twitter_sent_list = container3.Predict(twitter_data)

    container4 = xmlrpc.client.ServerProxy('http://0.0.0.0:12000')
    twitter_polarity_list = container4.Predict(twitter_sent_list)

    container5 = xmlrpc.client.ServerProxy('http://0.0.0.0:12000')
    prediction_1 = container5.Predict(stock_data)

    container6 = xmlrpc.client.ServerProxy('http://0.0.0.0:12000')
    prediction_2 = container6.Predict(stock_data)

    container7 = xmlrpc.client.ServerProxy('http://0.0.0.0:13000')
    prediction_3 = container7.Predict(stock_data)

    container8 = xmlrpc.client.ServerProxy('http://0.0.0.0:14000')
    final_prediction = container8.Predict(twitter_polarity_list, prediction_1, prediction_2, prediction_3)

    print(final_prediction)


if __name__ == "__main__":
    run()