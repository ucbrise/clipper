package ai.clipper.examples.container;

import ai.clipper.container.ClipperModel;
import ai.clipper.container.data.DataType;
import ai.clipper.container.data.FloatVector;
import ai.clipper.container.data.SerializableString;

import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Iterator;

public class NoOpStringModel extends ClipperModel<SerializableString> {
  public NoOpStringModel() {}

  @Override
  public FloatVector predict(ArrayList<SerializableString> inputVector) {
    float[] responses = new float[inputVector.size()];
    int index = 0;
    for (SerializableString s : inputVector) {
      responses[index] = s.getData().length();
    }
    return new FloatVector(FloatBuffer.wrap(responses));
  }

  @Override
  public DataType getInputType() {
    return DataType.Strings;
  }
}
