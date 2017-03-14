package data;

import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;
import java.nio.ByteOrder;

public class DoubleVector extends DataVector<double[]> {
  public DoubleVector(double[] data) {
    super(data);
  }

  @Override
  public byte[] toBytes() {
    return DataUtils.getBytesFromDoubles(data);
  }

  public static class Parser extends DataVectorParser<double[], DoubleVector> {
    @Override
    DoubleVector constructDataVector(double[] data) {
      return new DoubleVector(data);
    }

    @Override
    DataBuffer<double[]> getDataBuffer() {
      return new DataBuffer<double[]>() {

        DoubleBuffer buffer;

        @Override
        void init(ByteBuffer buffer) {
          buffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = buffer.asDoubleBuffer();
        }

        @Override
        double[] get(int offset, int size) {
          double[] data = new double[size];
          buffer.get(data, offset, size);
          return data;
        }

        @Override
        double[] getAll() {
          int size = buffer.remaining();
          double[] data = new double[size];
          buffer.get(data, 0, size);
          return data;
        }
      };
    }
  }
}
