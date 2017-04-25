package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DataVector;
import clipper.container.app.data.SerializableString;

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
    return DataType.Strings;
  }

  /**
   * @return A JSON-formatted serializable string to be
   * returned to Clipper as a prediction result
   */
  public abstract SerializableString predict(I inputVector);
}
