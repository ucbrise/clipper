package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DataVector;
import clipper.container.app.data.SerializableString;

import java.nio.Buffer;

public class NoOpModel<T extends DataVector<Buffer>> extends Model<T> {
  public NoOpModel(String name, int version, DataType inputType) {
    super(name, version, inputType);
  }

  @Override
  public SerializableString predict(T inputVector) {
    String jsonContent = String.format("{ \"data_size\": %d }", inputVector.getData().remaining());
    return new SerializableString(jsonContent);
  }
}
