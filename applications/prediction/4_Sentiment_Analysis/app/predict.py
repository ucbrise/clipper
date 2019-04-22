import nltk
nltk.download('vader_lexicon')

def predict(sent_list):

    from nltk.sentiment.vader import SentimentIntensityAnalyzer

    nltk_sentiment = SentimentIntensityAnalyzer()

    result_list = []

    for sent in sent_list:
        result_list.append(nltk_sentiment.polarity_scores(sent))

    return result_list