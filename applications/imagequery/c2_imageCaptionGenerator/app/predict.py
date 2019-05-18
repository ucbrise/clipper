import os
import json
from timeit import default_timer as timer

def find(name, path):
  for root, dirs, files in os.walk(path):
    if name in files:
      return os.path.join(root, name)


# Run run_inference.py
run_inference_path = find("run_inference.py", "/")
check_point_path = find("newmodel.ckpt-2000000","/")
vocabulary_path = find("word_counts.txt","/")
image_path = find("image.jpg","/")
print(run_inference_path)
print(check_point_path)
print(vocabulary_path)
print(image_path)
# cmd = "python " + run_inference_path + " --checkpoint_path " + check_point_path + " --vocab_file " + vocabulary_path + " --input_files " + image_path
# os.system(cmd)

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

if __name__ == "__main__":
    predict("image.jpg")
