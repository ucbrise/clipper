import data.DataType;
import data.FloatVector;
import data.SerializableString;

import java.util.ArrayList;
import java.util.List;

public class NoOpStringModel extends Model<SerializableString, FloatVector> {
  NoOpStringModel(String name, int version) {
    super(name, version, DataType.Strings, DataType.Floats);
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
