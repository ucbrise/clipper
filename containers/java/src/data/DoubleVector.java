package data;

import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;

public class DoubleVector extends DataVector<double[]> {

    public DoubleVector(double[] data) {
        super(data);
    }

    @Override
    public byte[] toBytes() {
        return DataUtils.getBytesFromDoubles(data);
    }

    public static class Parser extends DataVectorParser<double[], DoubleVector> {

        @Override
        DoubleVector constructDataVector(double[] data) {
            return new DoubleVector(data);
        }

        @Override
        DataBuffer<double[]> getDataBuffer() {
            return new DataBuffer<double[]>() {

                DoubleBuffer buffer;

                @Override
                void init(ByteBuffer buffer) {
                    this.buffer = buffer.asDoubleBuffer();
                }

                @Override
                double[] get(int offset, int size) {
                    double[] data = new double[size];
                    buffer.get(data, offset, size);
                    return data;
                }
            };
        }
    }

}
