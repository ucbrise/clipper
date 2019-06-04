import time
import speech_recognition as sr
# Reference: https://realpython.com/python-speech-recognition/

recognizer = sr.Recognizer()
print("Finish preloading speech recognizer!")

def recognize(audio_file_index):
    audio_file_index = int(audio_file_index)
    if audio_file_index < 0 or audio_file_index > 1000:
        return "Invalid image index! Only index between 1 to 1000 is allowed! Exiting..."

    dataset_index = 3
    if dataset_index == 1:
        # dataset1: CMU arctic
        audio_file_path = "/container/container1/data/dataset1/cmu_us_awb_arctic/wav/" + str(audio_file_index) + ".wav"
    elif dataset_index == 2:
        # dataset2: Flicker: different scripts but with overlapping
        audio_file_path = "/container/container1/data/dataset2/flickr_audio/wavs/" + str(audio_file_index) + ".wav"
    elif dataset_index == 3:
        # dataset3: speech-accent-archive: different people reading the same script
        audio_file_path = "/container/container1/data/dataset3/recordings/" + str(audio_file_index) + ".wav"
    else:
        return "Invalid dataset index!"

    audio_file = sr.AudioFile(audio_file_path)

    with audio_file as source:
        audio = recognizer.record(source)

    recognized_str = recognizer.recognize_google(audio)

    return recognized_str + ". "


def predict(audio_file_index):
    recognized_string = recognize(audio_file_index)
    return recognized_string

