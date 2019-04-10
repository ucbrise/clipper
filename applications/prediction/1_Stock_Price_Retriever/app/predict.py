# originally named stockPriceRetriever.py
# this program loads stock price data and saves to file
import quandl
import datetime

quandl.ApiConfig.api_key = "ZFtsDc5JcPvNXWwFVTSR"

def retrieveStockPrice(stockcode):
  start = datetime.datetime(2017, 1, 1)
  end = datetime.date.today()
  stock_price_dataframe = quandl.get("WIKI/" + stockcode, start_date=start, end_date=end) # dateframe type
  return stock_price_dataframe.to_json()

def predict(stockcode): # serves as an api function
  return retrieveStockPrice(stockcode)