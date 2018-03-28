package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;

public class DoubleVector extends DataVector<DoubleBuffer> {
  public DoubleVector(DoubleBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<DoubleBuffer, DoubleVector> {
    @Override
    public DoubleVector constructDataVector(ByteBuffer data, long byteSize) {
      DoubleBuffer doubleData = data.asDoubleBuffer();
      doubleData.limit((int) (byteSize / Double.BYTES));
      return new DoubleVector(doubleData);
    }
  }
}
