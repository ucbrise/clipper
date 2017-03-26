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
  public FloatVector predict(SerializableString inputVector) {
    return new FloatVector(new float[] {(float) inputVector.getData().length()});
  }
}
