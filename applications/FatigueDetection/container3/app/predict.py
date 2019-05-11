import numpy as np
#import skimage.io
#import matplotlib as mpl
#mpl.use('TkAgg')
#import matplotlib.pyplot as plt
import cv2
import json
from samples.coco import coco
import mrcnn.model as modellib
#import imgaug

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

class_names = ['BG', 'person', 'bicycle', 'car', 'motorcycle', 'airplane',
               'bus', 'train', 'truck', 'boat', 'traffic light',
               'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird',
               'cat', 'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear',
               'zebra', 'giraffe', 'backpack', 'umbrella', 'handbag', 'tie',
               'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
               'kite', 'baseball bat', 'baseball glove', 'skateboard',
               'surfboard', 'tennis racket', 'bottle', 'wine glass', 'cup',
               'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
               'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza',
               'donut', 'cake', 'chair', 'couch', 'potted plant', 'bed',
               'dining table', 'toilet', 'tv', 'laptop', 'mouse', 'remote',
               'keyboard', 'cell phone', 'microwave', 'oven', 'toaster',
               'sink', 'refrigerator', 'book', 'clock', 'vase', 'scissors',
               'teddy bear', 'hair drier', 'toothbrush']


class InferenceConfig(coco.CocoConfig):
    # Set batch size to 1 since we'll be running inference on
    # one image at a time. Batch size = GPU_COUNT * IMAGES_PER_GPU
    GPU_COUNT = 1
    IMAGES_PER_GPU = 1

config = InferenceConfig()
config.display()

# Create model object in inference mode.
model = modellib.MaskRCNN(mode="inference", model_dir="logs", config=config)

# Load weights trained on MS-COCO
model.load_weights("container/mask_rcnn_coco.h5", by_name=True)

def predict(imstr):
    image=string_image(imstr)
    
    # Run detection
    results = model.detect([image], verbose=1)
    
    # Visualize results
    r = results[0]
    if 1 in r['class_ids']:
        pos=r['class_ids'].tolist().index(1)
        r['class_ids']=np.array([1])
        r['masks']=r['masks'][:,:,pos:pos+1]
        r['rois']=np.array([r['rois'][pos]])
        r['scores']=np.array([r['scores'][pos]])
    else:
        return None
    prediction=make_box_mask(image, r['rois'].tolist()[0])
    imagestring=image_string(prediction)
    return imagestring
    
    
#visualize.display_instances(image, r['rois'], r['masks'], r['class_ids'], class_names, r['scores'])

def make_box_mask(image, xy):    
    target = image[xy[0]:xy[2], xy[1]:xy[3], :]
    img = np.zeros_like(image)
    img[xy[0]:xy[2], xy[1]:xy[3], :] = target
    return target
#    cv2.imwrite("masked.jpg",target)
#    plt.imshow(img)
    
#image = cv2.imread("simple.jpg")
#string=image_string(image)
#x=predict(string)








