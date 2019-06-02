from multiprocessing import Pool
from timeit import default_timer as timer

import c0_entryContainer.predict as entry_container
import c1_speechRecognition.predict as speech_recognizer
import c2_imageCaptionGenerator.predict as caption_generator
import c3_nlpMappingGenerator.predict as mapping_generator
import c4_questionAnswering.predict as question_answerer
print("---Modules successfully imported---")

def run_speech_recognition(input_index):
  speech_text, elapsed_time = speech_recognizer.predict(input_index)
  print("1:\tText: " + speech_text)
  return speech_text, elapsed_time

def generate_image_caption(input_index):
  captions, elapsed_time = caption_generator.predict(input_index)
  print("2:\tGenerated captions: " + captions)
  return captions, elapsed_time
		
def run(input_index):
  elapsed_time_list = []

  # CONTAINER 0
  input_index = entry_container.predict(input_index)

  # CONTAINER 1, 2: Multi Threading
  p = Pool(1) # use only one subprocess, run TF session in main process
  returned_result1 = p.apply_async(run_speech_recognition, args=(input_index,))
  # returned_result2 = p.apply_async(generate_image_caption, args=(input_index,))
  result2, time2 = generate_image_caption(input_index)
  p.close()
  p.join() # p.join()方法会等待所有子进程执行完毕

  result1 = returned_result1.get()[0]
  time1 = returned_result1.get()[1]
  # result2 = returned_result2.get()[0]
  # time2 = returned_result2.get()[1]
  elapsed_time_list.append(time1)
  elapsed_time_list.append(time2)

  # CONTAINER 1, 2: Synchronous
  # result1, time1 = speech_recognizer.predict(input_index)
  # print("1:\tText: " + result1)
  # result2, time2 = caption_generator.predict(input_index)
  # print("2:\tGenerated captions: " + result2)
  # elapsed_time_list.append(time1)
  # elapsed_time_list.append(time2)

  # CONTAINER 3
  text = result1 + "|" + result2
  mapping, elapsed_time = mapping_generator.predict(text)
  elapsed_time_list.append(elapsed_time)
  print("3:\tGenerated mapping: ")
  items = mapping.split('-')
  nouns = items[0]
  verbs = items[1]
  print("\t- Nouns: " + nouns)
  print("\t- Verb: " + verbs)

  # Container 4
  question = "Verb"
  answer, elapsed_time = question_answerer.predict(mapping)
  elapsed_time_list.append(elapsed_time)
  print("4:\tThe asked question is: " + question)
  print("\tGenerated answer: " + answer)

  print("Time elapsed for each container(second):")
  print("Speech Recognition:\t\t" , elapsed_time_list[0])
  print("Image Caption Generation:\t" , elapsed_time_list[1])
  print("NLP:\t\t\t\t" , elapsed_time_list[2])
  print("Question Answering:\t\t" , elapsed_time_list[3])

if __name__ == "__main__":
  start = timer()
  for i in range(50):
    run(i)
  end = timer()
  time_elapsed = end - start 
  print("Total time: " + str(time_elapsed) + " seconds.")

