import data.DataType;
import data.DoubleVector;
import data.FloatVector;

import java.util.ArrayList;
import java.util.List;

public class NoOpModel extends Model<DoubleVector> {
  NoOpModel(String name, int version) {
    super(name, version, DataType.Doubles);
  }

  @Override
  public List<FloatVector> predict(List<DoubleVector> inputVectors) {
    List<FloatVector> outputs = new ArrayList<FloatVector>();
    System.out.println("Predicting on " + inputVectors.size() + " inputs");
    // boolean printVec = true;
    double totalSumA = 0.0;
    float totalSumB = 0.0f;
    for (DoubleVector i : inputVectors) {
      float sum = 0.0f;
      for (double d : i.getData()) {
        sum += (float) d;
        totalSumA += d;
        // if (printVec) {
        //   System.out.print(d + " ");
        // }
      }
      totalSumB += sum;
      // if (printVec) {
      //   System.out.println("");
      // }
      // printVec = false;
      outputs.add(new FloatVector(new float[] {sum}));
    }
    System.out.println("Total sum A: " + totalSumA);
    System.out.println("Total sum B: " + totalSumB);
    return outputs;
  }
}
