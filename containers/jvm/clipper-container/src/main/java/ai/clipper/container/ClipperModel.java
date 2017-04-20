package ai.clipper.container;

import ai.clipper.container.data.DataType;
import ai.clipper.container.data.DataVector;
import ai.clipper.container.data.FloatVector;

import java.util.ArrayList;

public abstract class ClipperModel<I extends DataVector> {
  public abstract DataType getInputType();

  public DataType getOutputType() {
    return DataType.Floats;
  }

  public abstract ArrayList<SerializableString> predict(ArrayList<I> inputVector);
}
