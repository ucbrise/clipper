from PIL import Image
from resizeimage import resizeimage
# reference: https://github.com/VingtCinq/python-resize-image
import subprocess

def resize(image_name):
    image_directory_path = '/container/'
    resized_width = 200
    resized_height = 150
    with open(image_directory_path + image_name , 'r+b') as f:
        with Image.open(f) as image:
            cover = resizeimage.resize_cover(image, [resized_width, resized_height])
            cover.save(image_directory_path + 'resized_' + image_name, image.format)

    # check if the image is resized successfully
    resized_image_name = 'resized_' + image_name
    with open(image_directory_path + resized_image_name , 'r+b') as f:
        with Image.open(f) as image:
            print("resized successfully!!")
    

def predict(image_name):
    resize(image_name)


