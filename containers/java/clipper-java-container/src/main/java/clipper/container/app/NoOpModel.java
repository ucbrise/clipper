package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DataVector;
import clipper.container.app.data.FloatVector;
import clipper.container.app.data.SerializableString;
import org.kopitubruk.util.json.JSONUtil;

import java.nio.Buffer;
import java.nio.FloatBuffer;
import java.util.HashMap;
import java.util.Map;

public class NoOpModel<T extends DataVector<Buffer>> extends Model<T> {
  public NoOpModel(String name, int version, DataType inputType) {
    super(name, version, inputType);
  }

  @Override
  public SerializableString predict(T inputVector) {
    //Map<String, Float> jsonKeys = new HashMap<>();
    //jsonKeys.put("data_size", (float) inputVector.getData().remaining());
    //String jsonResponse = JSONUtil.toJSON(jsonKeys);
    return new SerializableString("isfsdi\0n");
  }
}
