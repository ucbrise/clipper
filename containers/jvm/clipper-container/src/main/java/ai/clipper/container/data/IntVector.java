package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;

public class IntVector extends DataVector<IntBuffer> {
  public IntVector(IntBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<IntBuffer, IntVector> {
    @Override
    public IntVector constructDataVector(ByteBuffer data, long byteSize) {
      IntBuffer intData = data.asIntBuffer();
      intData.limit((int) (byteSize / Integer.BYTES));
      return new IntVector(intData);
    }
  }
}
