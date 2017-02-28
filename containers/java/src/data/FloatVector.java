package data;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

public class FloatVector extends DataVector<float[]> {

    public FloatVector(float[] data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return DataUtils.getBytesFromFloats(data);
    }

    public static class Parser extends DataVectorParser<float[], FloatVector> {

        @Override
        FloatVector constructDataVector(float[] data) {
            return new FloatVector(data);
        }

        @Override
        DataBuffer<float[]> getDataBuffer() {
            return new DataBuffer<float[]>() {

                FloatBuffer buffer;

                @Override
                void init(ByteBuffer buffer) {
                    this.buffer = buffer.asFloatBuffer();
                }

                @Override
                float[] get(int offset, int size) {
                    float[] data = new float[size];
                    buffer.get(data, offset, size);
                    return data;
                }
            };
        }
    }

}
