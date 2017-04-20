package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.ByteOrder;

public class FloatVector extends DataVector<FloatBuffer> {
  public FloatVector(FloatBuffer data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<FloatBuffer, FloatVector> {
    @Override
    FloatVector constructDataVector(FloatBuffer data) {
      return new FloatVector(data);
    }

    @Override
    DataBuffer<FloatBuffer> createDataBuffer() {
      return new DataBuffer<FloatBuffer>() {

        FloatBuffer buffer;
        int bufferSize;

        @Override
        void init(ByteBuffer inputBuffer) {
          inputBuffer.order(ByteOrder.LITTLE_ENDIAN);
          this.buffer = inputBuffer.asFloatBuffer();
          this.bufferSize = buffer.remaining();
        }

        @Override
        FloatBuffer get(int size) {
          int outputLimit = buffer.position() + size;
          buffer.limit(outputLimit);
          FloatBuffer outputBuffer = buffer.slice();
          buffer.position(outputLimit);
          buffer.limit(bufferSize);
          return outputBuffer;
        }

        @Override
        FloatBuffer getAll() {
          return buffer;
        }
      };
    }
  }
}
