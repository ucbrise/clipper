package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;
import java.nio.ByteOrder;

public class DoubleVector extends DataVector<DoubleBuffer> {
  public DoubleVector(DoubleBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<DoubleBuffer, DoubleVector> {
    @Override
    DoubleVector constructDataVector(DoubleBuffer data) {
      return new DoubleVector(data);
    }

    @Override
    DataBuffer<DoubleBuffer> createDataBuffer() {
      return new DataBuffer<DoubleBuffer>() {

        DoubleBuffer buffer;
        int bufferSize;

        @Override
        void init(ByteBuffer inputBuffer) {
          inputBuffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = inputBuffer.asDoubleBuffer();
          this.bufferSize = buffer.remaining();
        }

        @Override
        DoubleBuffer get(int size) {
          int outputLimit = buffer.position() + size;
          buffer.limit(outputLimit);
          DoubleBuffer outputBuffer = buffer.slice();
          buffer.position(outputLimit);
          buffer.limit(bufferSize);
          return outputBuffer;
        }

        @Override
        DoubleBuffer getAll() {
          return buffer;
        }
      };
    }
  }
}
