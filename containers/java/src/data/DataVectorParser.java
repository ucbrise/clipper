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
        for(int split : splits) {
            U parsedArray = dataBuffer.get(0, split - prevSplit);
            parsedArrays.add(parsedArray);
            prevSplit = split;
        }
        return parsedArrays;
    }

    public List<T> parse(ByteBuffer byteBuffer, List<Integer> splits) {
        List<T> dataVectors = new ArrayList<>();
        List<U> data = parseBytes(byteBuffer, splits);
        for(U dataItem : data) {
            T dataVector = constructDataVector(dataItem);
            dataVectors.add(dataVector);
        }
        return dataVectors;
    }

}
