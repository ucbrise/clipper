package data;

import java.util.List;

public class IntVector extends DataVector<Integer> {

    public IntVector(List<Integer> data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return DataUtils.getBytesFromInts(data);
    }

    public static class Parser extends DataVectorParser<Integer, IntVector> {

        @Override
        List<Integer> parseBytes(byte[] bytes) {
            return DataUtils.getSignedIntsFromBytes(bytes);
        }

        @Override
        IntVector constructDataVector(List<Integer> data) {
            return new IntVector(data);
        }
    }

}
