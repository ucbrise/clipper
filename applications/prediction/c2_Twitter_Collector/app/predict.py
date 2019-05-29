import io
import sys
import tweepy
import time
from tweepy import *

def getData(keyword, limit):
  consumer_key='PLzMzrEMZOTi256ghZoTnY3S7'
  consumer_secret='Lql8oLrvhWnZQUhTMw7DImQJr5C0zlNpUker6sqIWNxaDsQPfI'
  access_token='1114411093160419328-YHKv2l7lcnzNdp6eOJOIaAayL3vNrw'
  access_token_secret='JxiiUA8aPth8cLr4PR35wEBW6HkJCwoiABFUpb98E75zh'

  auth = tweepy.OAuthHandler(consumer_key, consumer_secret)
  auth.set_access_token(access_token, access_token_secret)
  api = tweepy.API(auth)

  sys.stdout = io.TextIOWrapper(sys.stdout.buffer,encoding='utf8')
  #改变标准输出的默认编码, 否则print无法输出，因为有multiple byte character， 但是不影响代码运行

  tweets_string = ""
  for tweet in tweepy.Cursor(api.search, q = keyword, lang = "en" ).items(limit):
    tweets_string += tweet.text
    tweets_string += "\n"

  return tweets_string

def predict(request): # serve as api function
    start = time.time()
    info = request.split(":")
    stockcode = info[0]
    print("This is Twitter Collector")
    limit = 100
    to_return = getData(stockcode, limit)
    end = time.time()
    print("ELASPSED TIME", end - start)
    return to_return




