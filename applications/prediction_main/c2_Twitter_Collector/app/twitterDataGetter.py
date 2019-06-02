# NOT USED
# A TEMPLATE FOR REFERENCE ONLY

# http://adilmoujahid.com/posts/2014/07/twitter-analytics/
# https://stackoverflow.com/questions/38297150/twitter-streaming-stop-collecting-data
import tweepy
from tweepy import OAuthHandler
from tweepy import Stream
from tweepy.streaming import StreamListener
import sys
import json
import pandas as pd
import io 
import sys 

consumer_key='PLzMzrEMZOTi256ghZoTnY3S7'
consumer_secret='Lql8oLrvhWnZQUhTMw7DImQJr5C0zlNpUker6sqIWNxaDsQPfI'
access_token='1114411093160419328-YHKv2l7lcnzNdp6eOJOIaAayL3vNrw'
access_token_secret='JxiiUA8aPth8cLr4PR35wEBW6HkJCwoiABFUpb98E75zh'

auth = tweepy.OAuthHandler(consumer_key, consumer_secret)
auth.set_access_token(access_token, access_token_secret)
api = tweepy.API(auth)


tweets_data_path = './prediction/2_Twitter_Collector/app/twitter_data.txt'
# tweets_data_path = './container/twitter_data.txt'

class StdOutListener(StreamListener):
  tweet_number=0   # class variable

  def __init__(self,max_tweets):
    self.max_tweets= max_tweets # max number of tweets

  def on_data(self, data):
    self.tweet_number+=1
    if (self.tweet_number > self.max_tweets):
      return None
    try:
      tweet = json.loads(data)
      with open(tweets_data_path, 'a') as my_file:
        json.dump(tweet, my_file)
        my_file.write('\n')
    except BaseException:
      print('Error')
      pass
    if self.tweet_number >= self.max_tweets:
      print(str(self.max_tweets)+' tweets loaded! Stored in twitter_data.txt')
      return None
      # sys.exit(str(self.max_tweets)+' tweets loaded! Stored in twitter_data.txt')

  def on_error(self, status):
    print ("Error " + str(status))
    if status == 420:
      print("Rate Limited")
      return False

def loadDataIntoFile(keywordList):
  print(*keywordList)
  print("starting authenticating")
  l = StdOutListener(2)
  auth = OAuthHandler(consumer_key, consumer_secret)
  auth.set_access_token(access_token, access_token_secret)
  stream = Stream(auth, l)
  print("starting filtering")
  stream.filter(track=keywordList, languages=['en'])
  print("finishing loading")

# def useData(): # 还不能用
#   sys.stdout = io.TextIOWrapper(sys.stdout.buffer,encoding='utf8') #改变标准输出的默认编码, 否则print无法输出，因为有multiple byte character， 但是不影响代码运行
#   tweets_text_data = []
#   tweets_file = open(tweets_data_path, "r", encoding='utf-8')
#   for line in tweets_file:
#     try:
#       tweet = json.loads(line)
#       if 'text' in tweet: # only messages containing 'text' field is a tweet
#         tweets_text_data.append(tweet['text'])
#     except:
#         continue
#   tweets_text_data_string = "\n".join(tweets_text_data)
#   return tweets_text_data_string  

# def getRelatedTweets(keywordList):
#   loadDataIntoFile(keywordList)
#   return(useData())

if __name__ == "__main__":
  loadDataIntoFile(['apple', 'Apple'])


  