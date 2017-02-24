package data;

import java.util.List;

public class FloatVector extends DataVector<Float> {

    public FloatVector(List<Float> data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return DataUtils.getBytesFromFloats(data);
    }

    public static class Parser extends DataVectorParser<Float, FloatVector> {

        @Override
        List<Float> parseBytes(byte[] bytes) {
            return DataUtils.getFloatsFromBytes(bytes);
        }

        @Override
        FloatVector constructDataVector(List<Float> data) {
            return new FloatVector(data);
        }
    }

}
