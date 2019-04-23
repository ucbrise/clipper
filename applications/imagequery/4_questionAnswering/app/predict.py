import string
import numpy as np

def predict(question, mapping):
    print(mapping)
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
    print(counter)
    return(answer)


if __name__ == "__main__":
    print(predict("Who is in the pic?", "Dog-eat-day"))
    print(predict("What is in the pic?", "Dog-eat-day"))
    print(predict("What is the boy doing?", "Boy-play-night"))
