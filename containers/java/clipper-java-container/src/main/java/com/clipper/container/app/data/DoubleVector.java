package com.clipper.container.app.data;

import com.clipper.container.app.Pair;

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
    DataBuffer<double[]> createDataBuffer() {
      return new DataBuffer<double[]>() {

        DoubleBuffer buffer;
        double[] data = new double[INITIAL_BUFFER_SIZE];

        @Override
        void init(ByteBuffer buffer) {

          buffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = buffer.asDoubleBuffer();
        }

        @Override
        double[] get(int offset, int size) {
          if(size > data.length) {
            data = new double[data.length * 2];
          }
          buffer.get(data, offset, size);
          return data;
        }

        @Override
        double[] getAll() {
          int size = buffer.remaining();
          return get(0, size);
        }
      };
    }
  }
}
