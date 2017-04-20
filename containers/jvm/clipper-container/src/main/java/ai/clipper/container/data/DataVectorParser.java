package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Iterator;

public abstract class DataVectorParser<U, T extends DataVector<U>> {
  private DataBuffer<U> dataBuffer;

  abstract T constructDataVector(U data);

  abstract DataBuffer<U> createDataBuffer();

  private DataBuffer<U> getDataBuffer() {
    if (dataBuffer == null) {
      dataBuffer = createDataBuffer();
    }
    return dataBuffer;
  }

  public Iterator<T> parseDataVectors(ByteBuffer byteBuffer, IntBuffer splits) {
    return new DataVectorIterator(byteBuffer, splits);
  }

  class DataVectorIterator implements Iterator<T> {
    private IntBuffer splits;
    private ByteBuffer buffer;
    private int currentSplitIndex;
    private int prevSplit;

    DataVectorIterator(ByteBuffer buffer, IntBuffer splits) {
      this.splits = splits;
      this.buffer = buffer;
      this.currentSplitIndex = 0;
      this.prevSplit = 0;
      getDataBuffer().init(buffer);
    }

    @Override
    public boolean hasNext() {
      return (buffer != null) && (splits != null) && (currentSplitIndex >= 0)
          // If the split index is equivalent to the length
          // of the splits list, data must be processed
          // from the last split through the buffer's end
          && (currentSplitIndex <= splits.remaining());
    }

    @Override
    public T next() {
      DataBuffer<U> dataBuffer = getDataBuffer();
      U parsedArray;
      T dataVector;
      if (currentSplitIndex < splits.remaining()) {
        int currSplit = splits.get(currentSplitIndex);
        parsedArray = dataBuffer.get(currSplit - prevSplit);
        prevSplit = currSplit;
      } else {
        parsedArray = dataBuffer.getAll();
      }
      dataVector = constructDataVector(parsedArray);
      currentSplitIndex++;
      return dataVector;
    }
  }
}
