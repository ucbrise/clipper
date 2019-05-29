import speech_recognition as sr
from timeit import default_timer as timer
# Reference: https://realpython.com/python-speech-recognition/

recognizer = sr.Recognizer()

def recognize(audio_file_index):
    start = timer()

    audio_file_index = int(audio_file_index)
    if audio_file_index < 0 or audio_file_index > 1000:
        return "Invalid image index! Only index between 1 to 1000 is allowed! Exiting..."

    dataset_index = 3

    if dataset_index == 1:
        # dataset1: CMU arctic
        audio_file_path = "/container/data/dataset1/cmu_us_awb_arctic/wav/" + str(audio_file_index) + ".wav"
    elif dataset_index == 2:
        # dataset2: Flicker: different scripts but with overlapping
        audio_file_path = "/container/data/dataset2/flickr_audio/wavs/" + str(audio_file_index) + ".wav"
    elif dataset_index == 3:
        # dataset3: speech-accent-archive: different people reading the same script
        audio_file_path = "/container/data/dataset3/recordings/" + str(audio_file_index) + ".wav"
    else:
        return "Invalid dataset index!"

    print(audio_file_path)

    audio_file = sr.AudioFile(audio_file_path)

    with audio_file as source:
        audio = recognizer.record(source)

    recognized_str = recognizer.recognize_google(audio)

    end = timer()
    time_elapsed = end - start
    print(time_elapsed)

    return recognized_str


def predict(audio_file_path):
    
    recognized_string = recognize(audio_file_path)
    
    return recognized_string


if __name__ == "__main__":
    print(predict(1))

