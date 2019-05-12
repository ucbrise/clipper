# FatigueDetection

Structure see DAG description.png.

Input: Image, need to serialize to json string

Output: A boolean value indicating whether the driver is sleepling or not.

GPU:

open-cv: container 1-4

tensorflow: container 3



### Prerequisites

Before build the image, need to download the models from the google drive and put the model file into container#/app

container2 model: shape_predictor_68_face_landmarks.dat

link: https://drive.google.com/open?id=1srlfuqyX9zfZCxgnzSjWibZKv-U56WuC

container3 model: mask_rcnn_coco.h5

link: https://drive.google.com/open?id=1bNaOBthTNME64qkeKqNRYdMMogmZZJpc

container4 model: pose_iter_440000.caffemodel

link: https://drive.google.com/open?id=1uT6uv3a_J04O7UQfeGXKshJu5INRxigk

Dataset:

https://drive.google.com/file/d/0BwO0RMrZJCioaW5TdVJtOEtfYUk/view


