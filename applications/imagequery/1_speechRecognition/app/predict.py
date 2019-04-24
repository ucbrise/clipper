import speech_recognition as sr

def recognize(audio_file_path):
    recognizer = sr.Recognizer()

    audio_file = sr.AudioFile("/container/" + audio_file_path)

    with audio_file as source:
        audio = recognizer.record(source)

    recognized_str = recognizer.recognize_google(audio)
    return recognized_str


def predict(audio_file_path):
    return recognize(audio_file_path)


if __name__ == "__main__":
    print(recognize("C:/Users/musicman/Desktop/ZSX_Clipper/clipper/applications/imagequery/1_speechRecognition/app/speech.wav"))
