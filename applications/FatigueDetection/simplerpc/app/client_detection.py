import xmlrpc.client
import numpy as np
from scipy.io import wavfile
import cv2
import json

def image_string(image):
    image_encode=cv2.imencode('.jpg',image)[1]
    imagelist=image_encode.tolist()
    image_string=json.dumps(imagelist)
    return image_string

def string_image(imagestring):
    image_list=json.loads(imagestring)
    arr=np.array(image_list)
    arr=np.uint8(arr)
    image=cv2.imdecode(arr,cv2.IMREAD_COLOR)
    return image

def run():
    
    #container1 is a face extractor, output a image_data as string and memcached
    container1 = xmlrpc.client.ServerProxy('http://0.0.0.0:8000')
    image=cv2.imread('sleep.jpg')
    image_str=image_string(image)
    image_data = container1.Predict(image_str)

    print("\n[IOFO] Face Extraction FINISHED")
    if image_data==None:
        print("\n[INFO] No Person Detected In This Image, EXIT!")
        exit()
    
    #container2 is Drowsiness Detector, output a boolean value indicating whether the driver is sleeping or not, and memcached
#     container2 = xmlrpc.client.ServerProxy('http://0.0.0.0:9000')
#     drowsiness = container2.Predict(image_data)
#     print("\n[INFO] Drowsiness Detection FINISHED")
#     if drowsiness==True:
#         print("\n[PREDICTION] DROWSINESS ALERT!")
#     else:
#         print("\n[PREDICTION] No Drowsiness! Safe!")

#     container3 = xmlrpc.client.ServerProxy('http://0.0.0.0:11000')
#     human_segmentation = container3.Predict(image_str)
#     print("\n[INFO] Human Extraction FINISHED")
#     if human_segmentation==None:
#         print("\n[INFO] No Person Detected In This Image, EXIT!")
#         exit()

#     container4 = xmlrpc.client.ServerProxy('http://0.0.0.0:12000')
#     sleeping = container4.Predict(human_segmentation)
#     print("\n[INFO] Pose Analysis FINISHED")
#     if sleeping:
#         print("\n[PREDICTION] The Driver Is Likely To Be Sleeping!")
#     else:
#         print("\n[PREDICTION] The Driver Is NOT Likely To Be Sleepling.")


if __name__ == "__main__":
    run()
