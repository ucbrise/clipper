package com.clipper.container.app.data;

import com.clipper.container.app.Pair;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public abstract class DataVectorParser<U, T extends DataVector<U>> {

  private DataBuffer<U> dataBuffer;

  abstract T constructDataVector(U data);

  abstract DataBuffer<U> createDataBuffer();

  private DataBuffer<U> getDataBuffer() {
    if(dataBuffer == null) {
      dataBuffer = createDataBuffer();
    }
    return dataBuffer;
  }

  public Iterator<T> parseDataVectors(ByteBuffer byteBuffer, List<Integer> splits) {
    return new DataVectorIterator(byteBuffer, splits);
  }

  class DataVectorIterator implements Iterator<T> {

    private List<Integer> splits;
    private ByteBuffer buffer;
    private int currentSplitIndex;
    private int prevSplit;

    DataVectorIterator(ByteBuffer buffer, List<Integer> splits) {
      this.splits = splits;
      this.buffer = buffer;
      this.currentSplitIndex = 0;
      this.prevSplit = 0;
      getDataBuffer().init(buffer);
    }

    @Override
    public boolean hasNext() {
      return (buffer != null)
              && (splits != null)
              && (currentSplitIndex >= 0)
              // If the split index is equivalent to the length
              // of the splits list, data must be processed
              // from the last split through the buffer's end
              && (currentSplitIndex <= splits.size());
    }

    @Override
    public T next() {
      DataBuffer<U> dataBuffer = getDataBuffer();
      U parsedArray;
      T dataVector;
      if(currentSplitIndex < splits.size()) {
        int currSplit = splits.get(currentSplitIndex);
        parsedArray = dataBuffer.get(0, currSplit - prevSplit);
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
