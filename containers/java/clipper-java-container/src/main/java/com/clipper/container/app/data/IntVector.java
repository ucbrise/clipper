package data;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.ByteOrder;

public class IntVector extends DataVector<int[]> {
  public IntVector(int[] data) {
    super(data);
  }

  @Override
  public byte[] toBytes() {
    return DataUtils.getBytesFromInts(data);
  }

  public static class Parser extends DataVectorParser<int[], IntVector> {
    @Override
    IntVector constructDataVector(int[] data) {
      return new IntVector(data);
    }

    @Override
    DataBuffer<int[]> getDataBuffer() {
      return new DataBuffer<int[]>() {

        IntBuffer intBuffer;

        @Override
        void init(ByteBuffer buffer) {
          buffer.order(ByteOrder.LITTLE_ENDIAN);
          intBuffer = buffer.asIntBuffer();
        }

        @Override
        int[] get(int offset, int size) {
          int[] data = new int[size];
          intBuffer.get(data, offset, size);
          return data;
        }

        @Override
        int[] getAll() {
          int size = intBuffer.remaining();
          int[] data = new int[size];
          intBuffer.get(data, 0, size);
          return data;
        }
      };
    }
  }
}
