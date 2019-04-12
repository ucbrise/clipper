import nltk
nltk.download('vader_lexicon')

def predict(sent_list_str):

    from nltk.sentiment.vader import SentimentIntensityAnalyzer

    nltk_sentiment = SentimentIntensityAnalyzer()


    sent_list = sent_list_str.split(',')[:-1]

    result_list = []

    for sent in sent_list:
        result_list.append(nltk_sentiment.polarity_scores(sent))

    return str(result_list)