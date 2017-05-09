package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.SerializableString;

public class NoOpStringModel extends Model<SerializableString> {
  NoOpStringModel(String name, int version) {
    super(name, version, DataType.Strings);
  }

  @Override
  public SerializableString predict(SerializableString inputVector) {
    String jsonContent = String.format("{ \"data_size\": %d }", inputVector.getData().length());
    return new SerializableString(jsonContent);
  }
}