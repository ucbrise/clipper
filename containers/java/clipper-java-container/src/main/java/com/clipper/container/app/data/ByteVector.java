package com.clipper.container.app.data;

import java.nio.ByteBuffer;

public class ByteVector extends DataVector<byte[]> {
  public ByteVector(byte[] data) {
    super(data);
  }

  @Override
  public byte[] toBytes() {
    return data;
  }

  public static class Parser extends DataVectorParser<byte[], ByteVector> {
    @Override
    ByteVector constructDataVector(byte[] data) {
      return new ByteVector(data);
    }

    @Override
    DataBuffer<byte[]> createDataBuffer() {
      return new DataBuffer<byte[]>() {

        ByteBuffer buffer;
        byte[] data = new byte[INITIAL_BUFFER_SIZE];

        @Override
        void init(ByteBuffer buffer) {
          this.buffer = buffer;
        }

        @Override
        byte[] get(int offset, int size) {
          if(size > data.length) {
            data = new byte[data.length * 2];
          }
          buffer.get(data, offset, size);
          return data;
        }

        @Override
        byte[] getAll() {
          int size = buffer.remaining();
          return get(0, size);
        }
      };
    }
  }
}
