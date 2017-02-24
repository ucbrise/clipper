package data;

import java.util.List;

public class DoubleVector extends DataVector<Double> {

    public DoubleVector(List<Double> data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return DataUtils.getBytesFromDoubles(data);
    }

    public static class Parser extends DataVectorParser<Double, DoubleVector> {

        @Override
        List<Double> parseBytes(byte[] bytes) {
            return DataUtils.getDoublesFromBytes(bytes);
        }

        @Override
        DoubleVector constructDataVector(List<Double> data) {
            return new DoubleVector(data);
        }
    }

}
