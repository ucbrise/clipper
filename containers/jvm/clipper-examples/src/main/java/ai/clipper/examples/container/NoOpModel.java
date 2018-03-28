package ai.clipper.examples.container;

import ai.clipper.container.data.DataType;
import ai.clipper.container.data.DataVector;
import ai.clipper.container.ClipperModel;
import ai.clipper.container.data.SerializableString;

import java.nio.Buffer;
import java.util.ArrayList;

public class NoOpModel<T extends DataVector<Buffer>> extends ClipperModel<T> {
  private DataType inputType;

  public NoOpModel(DataType inputType) {
    this.inputType = inputType;
  }

  @Override
  public ArrayList<SerializableString> predict(ArrayList<T> inputVectors) {
    ArrayList<SerializableString> outputs = new ArrayList<>();
    for (T input : inputVectors) {
      String jsonContent = String.format("{ \"data_size\": %d }", input.getData().remaining());
      outputs.add(new SerializableString(jsonContent));
    }
    return outputs;
  }

  @Override
  public DataType getInputType() {
    return inputType;
  }
}
