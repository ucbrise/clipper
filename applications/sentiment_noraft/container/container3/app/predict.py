import nltk
import time
nltk.download('vader_lexicon')
from nltk.sentiment.vader import SentimentIntensityAnalyzer
# print(sent_list)
nltk_sentiment = SentimentIntensityAnalyzer()

def predict(sent_list):

    start = time.time()

    # sent_list is actually a string, containing words separated by -
    
    sent_list = sent_list.split("|||")

    # print(sent_list[:5])

    result_list = ""

    for sent in sent_list:
        score = nltk_sentiment.polarity_scores(sent)
        print(score)
        result_list += "|||" + str(score)

    end = time.time()
    
    print("ELASPSED TIME", end - start)

    return result_lis
