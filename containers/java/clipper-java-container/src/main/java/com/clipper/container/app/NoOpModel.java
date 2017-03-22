package com.clipper.container.app;

import com.clipper.container.app.data.DataType;
import com.clipper.container.app.data.DoubleVector;
import com.clipper.container.app.data.FloatVector;

import java.util.ArrayList;
import java.util.List;

class NoOpModel extends Model<DoubleVector> {

  NoOpModel(String name, int version) {
    super(name, version, DataType.Doubles);
  }

  @Override
  public List<FloatVector> predict(List<DoubleVector> inputVectors) {
    List<FloatVector> outputs = new ArrayList<FloatVector>();
    for (DoubleVector i : inputVectors) {
      float sum = 0.0f;
      for (double d : i.getData()) {
        sum += (float) d;
      }
      outputs.add(new FloatVector(new float[] {sum}));
    }
    return outputs;
  }
}
