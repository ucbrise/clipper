package com.clipper.container.app.data;

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
    DataBuffer<int[]> createDataBuffer() {
      return new DataBuffer<int[]>() {

        IntBuffer buffer;
        int[] data = new int[INITIAL_BUFFER_SIZE];

        @Override
        void init(ByteBuffer buffer) {
          buffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = buffer.asIntBuffer();
        }

        @Override
        int[] get(int offset, int size) {
          if(size > data.length) {
            data = new int[data.length * 2];
          }
          buffer.get(data, offset, size);
          return data;
        }

        @Override
        int[] getAll() {
          int size = buffer.remaining();
          return get(0, size);
        }
      };
    }
  }
}
