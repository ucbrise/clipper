package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DataVector;
import clipper.container.app.data.FloatVector;

abstract class Model<I extends DataVector> {
  private String name;
  private int version;
  DataType inputType;

  protected Model(String name, int version, DataType inputType) {
    this.name = name;
    this.version = version;
    this.inputType = inputType;
  }

  public String getName() {
    return name;
  }

  public int getVersion() {
    return version;
  }

  public DataType getInputType() {
    return inputType;
  }

  public DataType getOutputType() {
    return DataType.Floats;
  }

  public abstract FloatVector predict(I inputVector);
}
