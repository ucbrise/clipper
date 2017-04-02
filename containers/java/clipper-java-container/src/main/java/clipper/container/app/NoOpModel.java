package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DataVector;
import clipper.container.app.data.FloatVector;

import java.nio.Buffer;
import java.nio.FloatBuffer;

public class NoOpModel<T extends DataVector<Buffer>> extends Model<T> {
  public NoOpModel(String name, int version, DataType inputType) {
    super(name, version, inputType);
  }

  @Override
  public FloatVector predict(T inputVector) {
    return new FloatVector(
        FloatBuffer.wrap(new float[] {(float) inputVector.getData().remaining()}));
  }
}
