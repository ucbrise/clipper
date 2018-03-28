package ai.clipper.container.data;

import java.nio.ByteBuffer;

public class ByteVector extends DataVector<ByteBuffer> {
  public ByteVector(ByteBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<ByteBuffer, ByteVector> {
    @Override
    public ByteVector constructDataVector(ByteBuffer data, long byteSize) {
      data.limit((int) byteSize);
      return new ByteVector(data);
    }
  }
}
