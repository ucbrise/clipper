package com.clipper.container.app;

import com.clipper.container.app.data.DataType;
import com.clipper.container.app.data.FloatVector;
import com.clipper.container.app.data.SerializableString;

import java.util.ArrayList;
import java.util.List;

public class NoOpStringModel extends Model<SerializableString> {
  NoOpStringModel(String name, int version) {
    super(name, version, DataType.Strings);
  }

  @Override
  public List<FloatVector> predict(List<SerializableString> inputVectors) {
    List<FloatVector> outputs = new ArrayList<FloatVector>();
    for (SerializableString s : inputVectors) {
      outputs.add(new FloatVector(new float[] {(float) s.getData().length()}));
    }
    return outputs;
  }
}
