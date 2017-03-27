package com.clipper.container.app;

import com.clipper.container.app.data.DataType;
import com.clipper.container.app.data.DoubleVector;
import com.clipper.container.app.data.FloatVector;

import java.nio.FloatBuffer;

class NoOpModel extends Model<DoubleVector> {

  NoOpModel(String name, int version) {
    super(name, version, DataType.Doubles);
  }

  @Override
  public FloatVector predict(DoubleVector inputVector) {
    float sum = 0.0f;
    while(inputVector.getData().hasRemaining()) {
      sum += inputVector.getData().get();
    }
    return new FloatVector(FloatBuffer.wrap(new float[] {sum}));
  }
}
