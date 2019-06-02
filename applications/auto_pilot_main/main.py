import sys
sys.path.append("/container")

from multiprocessing import Pool

import 0_Entry_Point.app.predict as entry
import 1_Image_Preprocessing.app.predict as preprocessing
import 2_Obstacle_Detection.app.predict as obstacle_detection
import 3_Route_Planning.app.predict as route_planning
import 4_Algo1.app.predict as algo1
import 5_Algo2.app.predict as algo2
import 6_Conclusion.app.predict as conclusion

print("Modules successfully imported!")
		
def run():

    0_output = entry.predict("0***7***7")

    1_output = preprocessing.predict(0_output)

    print("Image Preprocessing Finished")

    2_output = obstacle_detection.predict(1_output)

    3_output = route_planning.predict(2_output)

    print("Route Planning Finished")

    returned_result_list = []
    p = Pool(2)
    returned_result_list.append(p.apply_async(algo1.predict, args=(3_output,))) 
    returned_result_list.append(p.apply_async(algo2.predict, args=(3_output,)))
    p.close()
    p.join()

    print(returned_result_list)


if __name__ == "__main__":
  run()
