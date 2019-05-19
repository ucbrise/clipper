# Import libraries
import os
import cv2
import numpy as np
import json

# Read the model
model = cv2.dnn.readNetFromCaffe('container/container1/app/deploy.prototxt','container/container1/app/weights.caffemodel')

#imagestring is a serialized .jpg encoded image string



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


def predict(imagestring):
    image=string_image(imagestring)
#    image=cv2.imread('simple.jpg')  
    count = 0
    (h,w)=image.shape[:2]
    blob = cv2.dnn.blobFromImage(cv2.resize(image, (300, 300)), 1.0, (300, 300), (104.0, 177.0, 123.0))
    model.setInput(blob)
    detections = model.forward()

    # Identify each face
    for i in range(0, detections.shape[2]):
        box = detections[0, 0, i, 3:7] * np.array([w, h, w, h])
        (startX, startY, endX, endY) = box.astype("int")
        confidence = detections[0, 0, i, 2]
        if (confidence > 0.5):
            count += 1
            frame = image[startY:endY, startX:endX]
            #memcached for all the faces detected
#            if not os.path.exists('faces'):
#                print("New directory created")
#                os.makedirs('faces')
#            cv2.imwrite('container' + '/faces/' + str(i) + '_' + 'face.jpg', frame)
    print("Extracted " + str(count) + " faces from all images")
    if count==0:
        print("[INFO] No face in this image!")
        return None
    image_str=image_string(frame)
    return image_str

#image=cv2.imread('sleep.jpg')
#image_encode=cv2.imencode('.jpg',image)[1]
#image_array=np.array(image_encode)
#image_string=image_array.tostring()
#predict(image_string)


