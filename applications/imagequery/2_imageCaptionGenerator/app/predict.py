#!/usr/local/bin/python3
import subprocess
import os
import json
from timeit import default_timer as timer

""" 
This app is hard to configure as it requires too much environment setting. So Try not to modify it. 
If you have to, whenever you change anything, say the name of the image you want to generate captions, 
search through the whole application to see if there's any other names you should change at the same time.

How this application works?
1. The first thing to notice is the Dockerfile2 for this container. We are creating a workspace for bazel 
under /container/workspace to reduce unnecessary bugs. The structure of directories are explained in the Dockerfile.

2. runWithArg.sh is like the interface for predict.py. It accepts the file_path of model checkpoint, word_count.txt and image path 
as argument from predict.py and in turn uses these arguments to call run_inference.py and that is where the real work for generating captions start.

3. The captions are generated in run_inference.py and are stored in /container/workspace/captionData/captionFile.txt. 
After finish executing run_inference.py, we read the content of captionFile.txt in predict.py and return the result in predict() to
the client.

"""

def generateCaption(image_name):
  # run the shellscript runWithArg.sh
  # runWithArg.sh is the same as run.sh, except that it accepts argument as the name of the file
  workspace_path = "/container/workspace/"
  checkpoint_path = workspace_path + "im2txt/model/newmodel.ckpt-2000000"
  wordscount_path = workspace_path + "im2txt/data/word_counts.txt"
  image_path = workspace_path + "im2txt/data/images/" + image_name
  command = "bash /container/workspace/im2txt/runWithArg.sh " + checkpoint_path + " " + wordscount_path + " " + image_path + " "
  print("before!")
  os.system(command)
  print("after!")

  # The caption data will be written to /container/captionData/captions.txt captionData
  # we read the content of caption.txt in captionData and return it here
  caption_json_path = workspace_path + "captionData/captionFile.txt"
  captions = ""
  with open(caption_json_path) as json_file:
    print("opened")
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