import sys
import time
sys.path.append("/container")

from multiprocessing import Pool

import c0_Entry_Point.app.predict as entry
import c1_Image_Preprocessing.app.predict as preprocessing
import c2_Obstacle_Detection.app.predict as obstacle_detection
import c3_Route_Planning.app.predict as route_planning
import c4_Algo1.app.predict as algo1
import c5_Algo2.app.predict as algo2
import c6_Conclusion.app.predict as conclusion

print("Modules successfully imported!")
		
def run():

    start = time.time()

    c0_output = entry.predict("0***7***7")

    print(c0_output)

    c1_output = preprocessing.predict(c0_output)

    print("Image Preprocessing Finished")

    c2_output = obstacle_detection.predict(c1_output)

    print("Obstacle Detection Finished")

    c3_output = route_planning.predict(c2_output)

    print("Route Planning Finished")

    returned_result_list = []
    returned_result_list.append(algo1.predict(c3_output))
    returned_result_list.append(algo2.predict(c3_output))

    print(returned_result_list)

    print("Total Time:", time.time()-start)


if __name__ == "__main__":
  run()
