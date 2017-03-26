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
  public FloatVector predict(DoubleVector inputVector) {
    float sum = 0.0f;
      for (double d : inputVector.getData()) {
        sum += (float) d;
      }
    return new FloatVector(new float[] {sum});
  }
}
