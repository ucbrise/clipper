import numpy as np
import cv2
import json
import sys

sys.path.append("/container")

from multiprocessing import Pool

# c7 is discarded in this file, import error

import container1.app.predict as c1
import container2.app.predict as c2
import container3.app.predict as c3
import container4.app.predict as c4
print("Modules successfully loaded!")

#helping function
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

def run_c1(imstr):
    result_fa = c1.predict(imstr)
    print("\nFace Extraction FINISHED")
    if result_fa==None:
        print("\n[INFO] No Person Detected In This Image!")
    return result_fa

def run_c2(imstr):
    result_drowsiness = c2.predict(imstr)
    print("\nFacial Point Detection FINISHED")
    return result_drowsiness

def run_c3(imstr):
    result_hu = c3.predict(imstr)
    print("\nHuman Segmentation FINISHED")
    if human_segmentation==None:
         print("\n[INFO] No Person Detected In This Image!")
    return result_hu

def run_c4(imstr):
    result_sleep = c4.predict(stock_data.to_json())
    print("\nPrediction using Regrerssion FINISHED")
    return result_sleep

def pipe1(imstr):
    result=run_c1(imstr)
    if result==None:
        return "No Person!"
    drowsiness=run_c2(result)
    if drowsiness:
        return "Drowsiness!"
    else
        return "No Drowsiness!"


def pipe2(imstr):
    result=run_c3(imstr)
    if result==None:
        return "No Person!"
    sleep=run_c4(result)
    if sleep:
        return "Sleeping!"
    else:
        return "No Sleeping"

def run():
    print("\nStart Detection: ")
    
    pipe1_result = []
    pipe2_result=[]
    count=0;
    for filename in os.listdir(part1):
        if count>50:
            break
        count+=1
        p = Pool(2)
        pipe1_result.append(p.apply_async(pipe1, args=(stock_data,)))
        pipe2_result.append(p.apply_async(pipe2, args=(stock_data,)))
        p.close()
        p.join() # p.join()方法会等待所有子进程执行完毕

    print("\nResult of PIPE1:")
    print(pipe1_result)
    print("\nResult of PIPE2:")
    print(pipe2_result)


if __name__ == "__main__":
    run()
