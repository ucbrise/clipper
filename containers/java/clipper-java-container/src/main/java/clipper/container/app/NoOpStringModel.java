package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.FloatVector;
import clipper.container.app.data.SerializableString;
import org.kopitubruk.util.json.JSONUtil;

import java.nio.FloatBuffer;
import java.util.HashMap;
import java.util.Map;

public class NoOpStringModel extends Model<SerializableString> {
  NoOpStringModel(String name, int version) {
    super(name, version, DataType.Strings);
  }

  @Override
  public SerializableString predict(SerializableString inputVector) {
    Map<String, Float> jsonKeys = new HashMap<>();
    jsonKeys.put("data_size", (float) inputVector.getData().length());
    String jsonResponse = JSONUtil.toJSON(jsonKeys);
    return new SerializableString(jsonResponse);
  }
}