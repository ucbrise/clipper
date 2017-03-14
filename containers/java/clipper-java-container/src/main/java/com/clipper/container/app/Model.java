import data.DataVector;
import data.FloatVector;
import data.DataType;
import java.util.List;

abstract class Model<I extends DataVector> {
  private String name;
  private int version;
  DataType inputType;

  Model(String name, int version, DataType inputType) {
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

  public abstract List<FloatVector> predict(List<I> inputVectors);
}
