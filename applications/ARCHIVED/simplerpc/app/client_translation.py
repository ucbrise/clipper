#import xmlrpc.client
import numpy as np
from scipy.io import wavfile

def run():

    container1 = xmlrpc.client.ServerProxy('http://0.0.0.0:8000')
    fs, data = wavfile.read('test.wav')
    print(fs)
    print(data)
    text_data = container1.Predict(fs, np.ndarray.tolist(data))

    print("\nTranscription FINISHED")
    print("Generated a string of length ", len(text_data), " from this audio file.")
    print("The first 200 chracters transcribed are :\n", text_data[0:100])

    container2 = xmlrpc.client.ServerProxy('http://0.0.0.0:9000')
    sent_list = container2.Predict(text_data)
    print("\n\nTokenization FINISHED")
    print("Generated a list containing ", len(sent_list), " sentences")
    print("The first sentence is :\n", sent_list[0])

    container3 = xmlrpc.client.ServerProxy('http://0.0.0.0:11000')
    polarity = container3.Predict(sent_list)
    print("\n\nSentimental Analysis FINISHED")
    print("Generated a boolean value indicating the polarity tendency.")
    if polarity:
        print("The sentiment analysis result is positive.\n")
    else:
        print("The sentiment analysis result is negative.\n")

    container4 = xmlrpc.client.ServerProxy('http://0.0.0.0:12000')
    short_report = container4.Predict(text_data)
    print("\n\nSubject Analysis FINISHED")
    print("Here is my short report")
    print(short_report)


if __name__ == "__main__":
    run()
