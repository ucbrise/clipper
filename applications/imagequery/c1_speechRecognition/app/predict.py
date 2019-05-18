import speech_recognition as sr
from timeit import default_timer as timer
# Reference: https://realpython.com/python-speech-recognition/
# Text2Speech converter: https://www.text2speech.org/

def recognize(audio_file_path):
    recognizer = sr.Recognizer()

    audio_file = sr.AudioFile(audio_file_path)

    with audio_file as source:
        audio = recognizer.record(source)

    recognized_str = recognizer.recognize_google(audio)
    return recognized_str


def predict(audio_file_path):
    start = timer()
    recognized_string = recognize(audio_file_path)
    end = timer()
    time_elapsed = end - start
    return recognized_string, time_elapsed


if __name__ == "__main__":
    print(recognize("C:/Users/musicman/Desktop/ZSX_Clipper/clipper/applications/imagequery/1_speechRecognition/app/speech.wav"))
