import subprocess
import os
import json
from timeit import default_timer as timer

# Run run_inference.py
os.system("python3 /container/workspace/im2txt/run_inference.py --checkpoint_path /container/workspace/im2txt/model/newmodel.ckpt-2000000 --vocab_file /container/workspace/im2txt/data/word_counts.txt --input_files /container/workspace/im2txt/data/images/image.jpg")

def generateCaption(image_name):
  # The caption data will be written to /container/captionData/captions.txt
  # we read the content of caption.txt in captionData and return it here
  caption_json_path = "/container/workspace/captionData/captionFile.txt"
  captions = ""
  with open(caption_json_path) as json_file:
    caption_string_restored = json.load(json_file)
    print("caption_string_restored: " + caption_string_restored)
    caption_json = json.loads(caption_string_restored)
    captions += caption_json['caption0']
    captions += " "
    captions += caption_json['caption1']
    captions += " "
    captions += caption_json['caption2']
  return captions

def predict(resized_image_path):
  start = timer()
  generated_caption = generateCaption(resized_image_path)
  end = timer()
  time_elapsed = end - start
  return generated_caption, time_elapsed

if __name__ == '__main__':
    predict("image.jpg")