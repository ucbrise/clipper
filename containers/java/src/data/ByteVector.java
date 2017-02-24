package data;

import java.util.List;

public class ByteVector extends DataVector<Byte> {

    public ByteVector(List<Byte> data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return new byte[0];
    }

    public static class Parser extends DataVectorParser<Byte, ByteVector> {

        @Override
        List<Byte> parseBytes(byte[] bytes) {
            return DataUtils.getBytesAsList(bytes);
        }

        @Override
        ByteVector constructDataVector(List<Byte> data) {
            return new ByteVector(data);
        }
    }

}
