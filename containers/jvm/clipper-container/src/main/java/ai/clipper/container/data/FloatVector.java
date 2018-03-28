package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

public class FloatVector extends DataVector<FloatBuffer> {
  public FloatVector(FloatBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<FloatBuffer, FloatVector> {
    @Override
    public FloatVector constructDataVector(ByteBuffer data, long byteSize) {
      FloatBuffer floatData = data.asFloatBuffer();
      floatData.limit((int) (byteSize / Float.BYTES));
      return new FloatVector(floatData);
    }
  }
}
