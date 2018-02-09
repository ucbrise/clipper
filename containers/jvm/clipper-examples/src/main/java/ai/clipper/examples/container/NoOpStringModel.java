package ai.clipper.examples.container;

import ai.clipper.container.ClipperModel;
import ai.clipper.container.data.DataType;
import ai.clipper.container.data.SerializableString;

import java.util.ArrayList;

public class NoOpStringModel extends ClipperModel<SerializableString> {
  public NoOpStringModel() {}

  @Override
  public ArrayList<SerializableString> predict(ArrayList<SerializableString> inputs) {
    ArrayList<SerializableString> outputs = new ArrayList<>();
    for (SerializableString input : inputs) {
      String jsonContent = String.format("{ \"data_size\": %d }", input.getData().length());
      outputs.add(new SerializableString(jsonContent));
    }
    return outputs;
  }

  @Override
  public DataType getInputType() {
    return DataType.Strings;
  }
}