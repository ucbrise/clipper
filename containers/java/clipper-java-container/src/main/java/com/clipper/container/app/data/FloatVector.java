package com.clipper.container.app.data;

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
    DataBuffer<float[]> createDataBuffer() {
      return new DataBuffer<float[]>() {

        FloatBuffer buffer;
        float[] data = new float[INITIAL_BUFFER_SIZE];

        @Override
        void init(ByteBuffer buffer) {
          buffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = buffer.asFloatBuffer();
        }

        @Override
        float[] get(int offset, int size) {
          if(size > data.length) {
            data = new float[data.length * 2];
          }
          buffer.get(data, offset, size);
          return data;
        }

        @Override
        float[] getAll() {
          int size = buffer.remaining();
          return get(0, size);
        }
      };
    }
  }
}
