import nltk
nltk.download('vader_lexicon')

def predict(sent_list):
    # sent_list is actually a string, containing words separated by -
    from nltk.sentiment.vader import SentimentIntensityAnalyzer

    print(sent_list)

    nltk_sentiment = SentimentIntensityAnalyzer()
    sent_list = sent_list.split("-")[1:]

    print(sent_list[:5])

    result_list = "result"

    for sent in sent_list:
        result_list = result_list + "-" + nltk_sentiment.polarity_scores(sent)

    return result_list