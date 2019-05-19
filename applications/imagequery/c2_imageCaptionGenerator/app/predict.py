import os
import json
from timeit import default_timer as timer

def find(name, path):
  for root, dirs, files in os.walk(path):
    if name in files or name in dirs:
      return os.path.join(root, name)

# Run run_inference.py, here the image.jpg is just the testing image for setting up the tensorflow environment
run_inference_path = find("run_inference.py", "/")
model_dir_path = find("model", "/container")
check_point_path = model_dir_path + "/newmodel.ckpt-2000000"
vocabulary_path = find("word_counts.txt","/")
image_path = find("image.jpg","/")
setupTensorflowEnvironmentCmd = "python " + run_inference_path + " --checkpoint_path " + check_point_path + " --vocab_file " + vocabulary_path + " --input_files " + image_path
os.system(setupTensorflowEnvironmentCmd)

def getGeneratedCaptions():
  # This is the helper functionS for retrieving generated data
  # we read the content of caption.txt in captionData and return it here
  caption_json_path = find("captionFile.txt","/")
  captions = ""
  with open(caption_json_path) as json_file:
    caption_string_restored = json.load(json_file)
    caption_json = json.loads(caption_string_restored)
    captions += caption_json['caption0']
    captions += " "
    captions += caption_json['caption1']
    captions += " "
    captions += caption_json['caption2']
  return captions

def predict(image_file_index):
  if image_file_index > 800:
    return "Invalid image file index! Only index between 1 to 800 is allowed!"
  
  start = timer()
  image_file_path = "/container/c2_imageCaptionGenerator/im2txt/data/imageDataset/101_ObjectCategories/image_" + str(image_file_index).zfill(4) + ".jpg"
  predictCmd = "python " + run_inference_path + " --checkpoint_path " + check_point_path + " --vocab_file " + vocabulary_path + " --input_files " + image_file_path
  os.system(predictCmd)
  generated_caption = getGeneratedCaptions()
  end = timer()
  time_elapsed = end - start
  return generated_caption, time_elapsed

if __name__ == "__main__":
    predict(2)
