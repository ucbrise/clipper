import speech_recognition as sr
from timeit import default_timer as timer
from pocketsphinx import pocketsphinx, Jsgf, FsgModel
import os

t1 = timer()

language_directory = "/container/models/wsj1"
acoustic_parameters_directory = os.path.join(language_directory, "acoustic-model")
language_model_file = os.path.join(language_directory, "language-model.lm.bin")
phoneme_dictionary_file = os.path.join(language_directory, "pronounciation-dictionary.dict")

config = pocketsphinx.Decoder.default_config()
# set the path of the hidden Markov model (HMM) parameter files
config.set_string("-hmm", acoustic_parameters_directory)
config.set_string("-lm", language_model_file)
config.set_string("-dict", phoneme_dictionary_file)
# disable logging (logging causes unwanted output in terminal)
config.set_string("-logfn", os.devnull)
config.set_int("-topn", 1)
config.set_int("-ds", 4)
config.set_int("-pl_window", 10)
config.set_int("-maxhmmpf", 1000)
config.set_int("-maxwpf", 5)

decoder = pocketsphinx.Decoder(config)

t2 = timer()

print("Preloading finished in " + str(t2-t1) + " seconds.")


def recognize(audio_file_index):
    audio_file_index = int(audio_file_index)
    if audio_file_index < 0 or audio_file_index > 1000:
        return "Invalid image index! Only index between 1 to 1000 is allowed! Exiting..."

    dataset_index = 1

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

    recognizer = sr.Recognizer()
    audio_file = sr.AudioFile(audio_file_path)

    with audio_file as source:
        audio = recognizer.record(source)

    tx = timer()

    raw_data = audio.get_raw_data(convert_rate=16000, convert_width=2)
    decoder.start_utt()  # begin utterance processing
    decoder.process_raw(raw_data, False, True)
    decoder.end_utt()  # stop utterance processing
    hypothesis = decoder.hyp()

    ty = timer()
    print("predict time", ty-tx, "\n\n")

    return hypothesis.hypstr


def predict(audio_file_path):
    # start = timer()
    recognized_string = recognize(audio_file_path)
    print(recognized_string)
    # end = timer()
    # time_elapsed = end - start
    return recognized_string


if __name__ == "__main__":
    predict(1)
