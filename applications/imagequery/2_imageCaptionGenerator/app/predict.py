#!/usr/local/bin/python3
import subprocess
import os
import json

def generateCaption(resized_image_name):
  # run the shellscript runWithArg.sh
  # runWithArg.sh is the same as run.sh, except that it accepts argument as the name of the file
  workspace_path = "/container/workspace/"
  checkpoint_path = workspace_path + "im2txt/model/newmodel.ckpt-2000000"
  wordscount_path = workspace_path + "im2txt/data/word_counts.txt"
  resized_image_path = workspace_path + "im2txt/data/images/" + resized_image_name
  command = "bash ./container/workspace/im2txt/runWithArg.sh " + checkpoint_path + " " + wordscount_path + " " + resized_image_path + " "
  os.system(command)

  # The caption data will be written to /container/captionData/captions.txt captionData
  # we read the content of caption.txt in captionData and return it here
  caption_json_path = workspace_path + "captionData/captions.txt"
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
  return generateCaption(resized_image_path)