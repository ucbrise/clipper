package data;

import java.nio.ByteBuffer;

public class ByteVector extends DataVector<byte[]> {

    public ByteVector(byte[] data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return data;
    }

    public static class Parser extends DataVectorParser<byte[], ByteVector> {
        @Override
        ByteVector constructDataVector(byte[] data) {
            return new ByteVector(data);
        }

        @Override
        DataBuffer<byte[]> getDataBuffer() {
            return new DataBuffer<byte[]>() {

                ByteBuffer buffer;

                @Override
                void init(ByteBuffer buffer) {
                    this.buffer = buffer;
                }

                @Override
                byte[] get(int offset, int size) {
                    byte[] data = new byte[size];
                    buffer.get(data, offset, size);
                    return data;
                }
            };
        }
    }

}
