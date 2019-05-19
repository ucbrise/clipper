import sys
sys.path.append("/container")

from multiprocessing import Pool

import c1_speechRecognition.app.predict as speech_recognizer
import c2_imageCaptionGenerator.app.predict as caption_generator
import c3_nlpMappingGenerator.app.predict as mapping_generator
import c4_questionAnswering.app.predict as question_answerer
print("Modules successfully imported!")

def run_speech_recognition(audio_file_path):
  speech_text, elapsed_time = speech_recognizer.predict(audio_file_path)
  print("1:\tText: " + speech_text)
  return speech_text, elapsed_time

def generate_image_caption(image_file):
  captions, elapsed_time = caption_generator.predict("image.jpg")
  print("2:\tGenerated captions: " + captions)
  return captions, elapsed_time
		
def run():
  elapsed_time_list = []
  result_list = []
  p = Pool(2)
  returned_result1 = p.apply_async(run_speech_recognition, args=("/container/c1_speechRecognition/data/cmu_us_awb_arctic/wav/arctic_a0001.wav",))
  returned_result2 = p.apply_async(generate_image_caption, args=("image.jpg",))
  p.close()
  p.join() # p.join()方法会等待所有子进程执行完毕

  result1 = returned_result1.get()[0]
  time1 = returned_result1.get()[1]
  result2 = returned_result2.get()[0]
  time2 = returned_result2.get()[1]
  elapsed_time_list.append(time1)
  elapsed_time_list.append(time2)

  # CONTAINER 3: image nlp analyzer
  text = result1 + " . " + result2
  # print("Input to mapping generator: " + text)
  mapping, elapsed_time = mapping_generator.predict(text)
  elapsed_time_list.append(elapsed_time)
  print("3:\tGenerated mapping: ")
  items = mapping.split('-')
  subject = items[0]
  verb = items[1]
  time = items[2]
  print("\t- Subject: " + subject)
  print("\t- Verb: " + verb)
  print("\t- Time: " + time)

  # Container 4: Question Answerings
  question = "What is in the image?"
  answer, elapsed_time = question_answerer.predict(question, mapping)
  elapsed_time_list.append(elapsed_time)
  print("4:\tThe asked question is: " + question)
  print("\tGenerated answer: " + answer)

  print("Time elapsed for each container(second):")
  print("Speech Recognition:\t\t" , elapsed_time_list[0])
  print("Image Caption Generation:\t" , elapsed_time_list[1])
  print("NLP:\t\t\t\t" , elapsed_time_list[2])
  print("Question Answering:\t\t" , elapsed_time_list[3])

if __name__ == "__main__":
  run()
