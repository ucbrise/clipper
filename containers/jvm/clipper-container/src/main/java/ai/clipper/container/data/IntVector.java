package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.ByteOrder;

public class IntVector extends DataVector<IntBuffer> {
  public IntVector(IntBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<IntBuffer, IntVector> {
    @Override
    IntVector constructDataVector(IntBuffer data) {
      return new IntVector(data);
    }

    @Override
    DataBuffer<IntBuffer> createDataBuffer() {
      return new DataBuffer<IntBuffer>() {

        IntBuffer buffer;
        int bufferSize;

        @Override
        void init(ByteBuffer inputBuffer) {
          inputBuffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = inputBuffer.asIntBuffer();
          this.bufferSize = buffer.remaining();
        }

        @Override
        IntBuffer get(int size) {
          int outputLimit = buffer.position() + size;
          buffer.limit(outputLimit);
          IntBuffer outputBuffer = buffer.slice();
          buffer.position(outputLimit);
          buffer.limit(bufferSize);
          return outputBuffer;
        }

        @Override
        IntBuffer getAll() {
          return buffer;
        }
      };
    }
  }
}
