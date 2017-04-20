package ai.clipper.examples.container;

import ai.clipper.container.data.DataType;
import ai.clipper.container.data.DataVector;
import ai.clipper.container.data.FloatVector;
import ai.clipper.container.ClipperModel;

import java.nio.Buffer;
import java.nio.FloatBuffer;
import java.util.ArrayList;

public class NoOpModel<T extends DataVector<Buffer>> extends ClipperModel<T> {
  private DataType inputType;

  public NoOpModel(DataType inputType) {
    this.inputType = inputType;
  }

  @Override
  public FloatVector predict(ArrayList<T> inputVector) {
    float[] responses = new float[inputVector.size()];
    int index = 0;
    for (T input : inputVector) {
      responses[index] = input.getData().remaining();
    }
    return new FloatVector(FloatBuffer.wrap(responses));
  }

  @Override
  public DataType getInputType() {
    return inputType;
  }

  @Override
  public DataType getOutputType() {
    return DataType.Floats;
  }
}
