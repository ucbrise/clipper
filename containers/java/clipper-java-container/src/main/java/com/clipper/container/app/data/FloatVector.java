package data;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.ByteOrder;

public class FloatVector extends DataVector<float[]> {
  public FloatVector(float[] data) {
    super(data);
  }

  @Override
  public byte[] toBytes() {
    return DataUtils.getBytesFromFloats(data);
  }

  public static class Parser extends DataVectorParser<float[], FloatVector> {
    @Override
    FloatVector constructDataVector(float[] data) {
      return new FloatVector(data);
    }

    @Override
    DataBuffer<float[]> getDataBuffer() {
      return new DataBuffer<float[]>() {

        FloatBuffer buffer;

        @Override
        void init(ByteBuffer buffer) {
          buffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = buffer.asFloatBuffer();
        }

        @Override
        float[] get(int offset, int size) {
          float[] data = new float[size];
          buffer.get(data, offset, size);
          return data;
        }

        @Override
        float[] getAll() {
          int size = buffer.remaining();
          float[] data = new float[size];
          buffer.get(data, 0, size);
          return data;
        }
      };
    }
  }
}
