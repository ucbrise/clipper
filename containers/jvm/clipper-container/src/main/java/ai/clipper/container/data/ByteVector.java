package ai.clipper.container.data;

import java.nio.ByteBuffer;

public class ByteVector extends DataVector<ByteBuffer> {
  public ByteVector(ByteBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<ByteBuffer, ByteVector> {
    @Override
    ByteVector constructDataVector(ByteBuffer data) {
      return new ByteVector(data);
    }

    @Override
    DataBuffer<ByteBuffer> createDataBuffer() {
      return new DataBuffer<ByteBuffer>() {

        ByteBuffer buffer;
        int bufferSize;

        @Override
        void init(ByteBuffer inputBuffer) {
          this.buffer = inputBuffer;
          this.bufferSize = buffer.remaining();
        }

        @Override
        ByteBuffer get(int size) {
          int outputLimit = buffer.position() + size;
          buffer.limit(outputLimit);
          ByteBuffer outputBuffer = buffer.slice();
          buffer.position(outputLimit);
          buffer.limit(bufferSize);
          return outputBuffer;
        }

        @Override
        ByteBuffer getAll() {
          return buffer;
        }
      };
    }
  }
}
