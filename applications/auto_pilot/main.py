import sys
sys.path.append("/container")

from multiprocessing import Pool

import 1_Framce_Feeder.app.predict as frame_feeder
import 2_Algo1.app.predict as algo1
import 3_Algo2.app.predict as algo2
import 4_Algo3.app.predict as algo3
import 5_Algo4.app.predict as algo4
import 6_Conclusion as conclusion

print("Modules successfully imported!")

		
def run():

  smoothed_angle = 0

  img = cv2.imread('steering_wheel_image.jpg', 0)
  rows, cols = img.shape

  for i in range(1, 2000):

    image_str = frame_feeder.Predict(i)

    angle1 = float(algo1.Predict(image_str))
    print("The steering angle prediction result from the first algo is ", round(angle1, 4))

    angle2 = float(algo2.Predict(image_str))
    print("The steering angle prediction result from the second algo is ", round(angle2, 4))

    angle3 = float(algo3.Predict(image_str))
    print("The steering angle prediction result from the third algo is ", round(angle3, 4))

    angle4 = float(algo4.Predict(image_str))
    print("The steering angle prediction result from the fourth algo is ", round(angle4, 4))

    final_angle = float(conclusion.Predict(str(angle1), str(angle2), str(angle3), str(angle4)))
    print("The FINAL steering angle prediction is ", round(final_angle, 4), "\n")

    smoothed_angle += 0.2 * pow(abs((final_angle - smoothed_angle)), 2.0 / 3.0) * (final_angle - smoothed_angle) / \
                      abs(final_angle - smoothed_angle)

    M = cv2.getRotationMatrix2D((cols / 2, rows / 2), -smoothed_angle, 1)

    dst = cv2.warpAffine(img, M, (cols, rows))

    cv2.imshow("steering wheel", dst)


if __name__ == "__main__":
  run()
