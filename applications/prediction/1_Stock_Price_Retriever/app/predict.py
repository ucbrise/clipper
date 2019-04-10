# originally named stockPriceRetriever.py
# this program loads stock price data and saves to file
import pandas as pd
import numpy as np
import quandl
import datetime
import csv
import os
from io import StringIO
from pandas import DataFrame

def retrieveStockPrice(stockcode):
  # dest_csv_file_name = "/container/prediction/6_Mem/" + stockcode + '_stockPrice.csv'

  start = datetime.datetime(2016, 1, 1)
  end = datetime.date.today()
  stock_price_dataframe = quandl.get("WIKI/" + stockcode, start_date=start, end_date=end).head() # dateframe type

  print(stock_price_dataframe) # Date Column is the index
  stock_price_string = stock_price_dataframe.to_string(index=False)
  print(stock_price_string)

  # stock_price_dataframe.to_csv( dest_csv_file_name, )
  # print("Stock price saved to: ", dest_csv_file_name , "!\n")
  return stock_price_string

  # how to read a csv to pandas.DataFrame type? apple = pd.read_csv('AAPL_stockPrice.csv', skiprows=2)

def predict(stockcode): # serves as api function
  return retrieveStockPrice(stockcode)

if __name__ == "__main__":
  retrieveStockPrice('AAPL')