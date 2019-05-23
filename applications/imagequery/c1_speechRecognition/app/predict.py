import speech_recognition as sr
from timeit import default_timer as timer
# Reference: https://realpython.com/python-speech-recognition/
# Text2Speech converter: https://www.text2speech.org/

def recognize(audio_file_index):
    audio_file_index = int(audio_file_index)
    if audio_file_index < 1 or audio_file_index > 800:
        return "Invalid image index! Only index between 1 to 800 is allowed! Exiting..."

    if audio_file_index <= 500: 
        audio_file_path = "/container/data/cmu_us_awb_arctic/wav/arctic_a" + str(audio_file_index).zfill(4) + ".wav"
    else:
        audio_file_path = "/container/data/cmu_us_awb_arctic/wav/arctic_b" + str(audio_file_index - 500).zfill(4) + ".wav"

    print(audio_file_path)
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
    print(recognize(1))
