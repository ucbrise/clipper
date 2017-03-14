package data;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

public abstract class DataVectorParser<U, T extends DataVector<U>> {
  abstract T constructDataVector(U data);

  abstract DataBuffer<U> getDataBuffer();

  private List<U> parseBytes(ByteBuffer byteBuffer, List<Integer> splits) {
    DataBuffer<U> dataBuffer = getDataBuffer();
    dataBuffer.init(byteBuffer);
    List<U> parsedArrays = new ArrayList<>();
    int prevSplit = 0;
    // System.out.println(splits);
    for (int split : splits) {
      U parsedArray = dataBuffer.get(0, split - prevSplit);
      parsedArrays.add(parsedArray);
      prevSplit = split;
    }
    // System.out.println("array length: " + byteBuffer.array().length);
    // System.out.println("prevSplit: " + prevSplit);
    // System.out.print("last size: ");
    // System.out.println(byteBuffer.array().length - prevSplit);
    // get the last input
    U parsedArray = dataBuffer.getAll();
    parsedArrays.add(parsedArray);
    return parsedArrays;
  }

  public List<T> parse(ByteBuffer byteBuffer, List<Integer> splits) {
    List<T> dataVectors = new ArrayList<>();
    List<U> data = parseBytes(byteBuffer, splits);
    for (U dataItem : data) {
      T dataVector = constructDataVector(dataItem);
      dataVectors.add(dataVector);
    }
    return dataVectors;
  }
}
