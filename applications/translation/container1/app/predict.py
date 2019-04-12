import os
import numpy as np
import speech_recognition as sr
from os import path
from scipy.io import wavfile


def predict(data):

    #tup = data.split('-')
    #fs = tup[0]
    #audio_data = tup[1]

 

    local_audio_file_name = os.path.join(path.dirname(path.realpath(__file__)), "test.wav")

    #wavfile.write(local_audio_file_name, fs, np.asarray(audio_data, dtype=np.int16))

    r = sr.Recognizer()

    with sr.AudioFile(local_audio_file_name) as source:
        audio = r.record(source)

    try:
        text_data = r.recognize_sphinx(audio)
        return text_data
    except sr.UnknownValueError:
        print("Sphinx could not understand audio")
    except sr.RequestError as e:
        print("Sphinx error; {0}".format(e))