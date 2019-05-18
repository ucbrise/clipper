import sys
sys.path.append("/container")

from multiprocessing import Pool

import c1_speechRecognition.app.predict as speech_recognizer
import c2_imageCaptionGenerator.app.predict as caption_generator
import c3_nlpMappingGenerator.app.predict as mapping_generator
import c4_questionAnswering.app.predict as question_answerer
print("Modules successfully imported!")

def run():
    elapsed_time_list = []

    # CONTAINER 1: speech to text
    audio_file_name = "/container/c1_speechRecognition/app/speech.wav"
    speech_text, elapsed_time = speech_recognizer.predict(audio_file_name)
    elapsed_time_list.append(elapsed_time)
    print("1:\tText: " + speech_text)

    # CONTAINER 2: image caption generator
    captions, elapsed_time = caption_generator.predict("image.jpg")
    elapsed_time_list.append(elapsed_time)
    print("2:\tGenerated captions: " + captions)

    # CONTAINER 3: image nlp analyzer
    text =  captions + ". " + speech_text + "."
    mapping, elapsed_time = mapping_generator.predict(text)
    elapsed_time_list.append(elapsed_time)
    print("3:\tGenerated mapping: ")
    items = mapping.split('-')
    subject = items[0]
    verb = items[1]
    time = items[2]
    print("\t\tSubject: " + subject)
    print("\t\tVerb: " + verb)
    print("\t\tTime: " + time)

    # Container 4: Question Answerings
    question = "What is in the image?"
    answer, elapsed_time = question_answerer.predict(question, mapping)
    elapsed_time_list.append(elapsed_time)
    print("4:\tThe asked question is: " + question)
    print("\t\tGenerated answer is: " + answer)

    print("Time elapsed for each container:")
    print("Speech Recognition:\t\t" , elapsed_time_list[0])
    print("Image Caption Generation:\t" , elapsed_time_list[1])
    print("NLP:\t\t\t\t" , elapsed_time_list[2])
    print("Question Answering:\t\t" , elapsed_time_list[3])

if __name__ == "__main__":
    run()