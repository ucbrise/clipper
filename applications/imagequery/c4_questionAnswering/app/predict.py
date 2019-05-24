import string
import numpy as np
from timeit import default_timer as timer

"""
This is the preliminary version of Questiona Answering System. 
We are not using AI here but just use simple logic. 
The reason is that the context where we need to generate answer from is
short and simple and there are not too much information. Answering with 
logic already provides good enough result.
"""

def predict(mapping):
    start = timer()

    question = "What is in the picture?"
    question = question.lower()
    words = question.split(' ')
    counter = [0, 0, 0]  # subject, verb, time
    for word in words:
        if word == 'who' or word == 'what':
          counter[0] = counter[0] + 1
        if word == 'when':
          counter[2] = counter[2] + 1
        if word == 'doing':
          counter[1] = counter[1] + 1
          counter[0] = counter[0] - 1
    index = np.argmax(counter)
    items = mapping.split('-')
    answer = items[index]
    print(index)

    end = timer()
    time_elapsed = end - start
    return answer



if __name__ == "__main__":
    print(predict("Who is in the pic?", "Dog-eat-day"))
    print(predict("What is in the pic?", "Dog-eat-day"))
    print(predict("What is the boy doing?", "Boy-play-night"))
