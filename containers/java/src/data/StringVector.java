package data;

import java.util.List;

public class StringVector extends DataVector<String> {

    public StringVector(List<String> data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return new byte[0];
    }

    public static class Parser extends DataVectorParser<String, StringVector> {

        @Override
        List<String> parseBytes(byte[] bytes) {
            return null;
        }

        @Override
        StringVector constructDataVector(List<String> data) {
            return new StringVector(data);
        }
    }
}
