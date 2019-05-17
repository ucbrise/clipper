import sys
sys.path.append("..")

import c1_speechRecognition.app.predict as speech_recognition
import c2_imageCaptionGenerator.app.predict as caption_generator
import c3_nlpMappingGenerator as mapping_generator
import c4_questionAnswering as question_answerer

print("1:\t" , speech_recognition.predict("./speech.wav")[0])
print("2:\t" , caption_generator.predict("./speech.wav")[0])
