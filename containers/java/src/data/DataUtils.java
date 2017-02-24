package data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.Function;

public class DataUtils {

    private static ByteBuffer getByteBuffer(byte[] bytes) {
        ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        return buffer;
    }

    public static <T> byte[] getBytesFromType(List<T> data, int itemSize, BiFunction<T, ByteBuffer, Void> func) {
        ByteBuffer byteBuffer = ByteBuffer.allocate(data.size() * itemSize).order(ByteOrder.LITTLE_ENDIAN);
        for(T item : data) {
            func.apply(item, byteBuffer);
        }
        return byteBuffer.array();
    }

    public static byte[] getBytesFromInts(List<Integer> data) {
        return getBytesFromType(data, 4, new BiFunction<Integer, ByteBuffer, Void>() {
            @Override
            public Void apply(Integer item, ByteBuffer byteBuffer) {
                byteBuffer.putInt(item);
                return null;
            }
        });
    }

    public static byte[] getBytesFromFloats(List<Float> data) {
        return getBytesFromType(data, 4, new BiFunction<Float, ByteBuffer, Void>() {
            @Override
            public Void apply(Float item, ByteBuffer byteBuffer) {
                byteBuffer.putFloat(item);
                return null;
            }
        });
    }

    public static byte[] getBytesFromDoubles(List<Double> data) {
        return getBytesFromType(data, 8, new BiFunction<Double, ByteBuffer, Void>() {
            @Override
            public Void apply(Double item, ByteBuffer byteBuffer) {
                byteBuffer.putDouble(item);
                return null;
            }
        });
    }

    public static <T> List<T> getTypeFromBytes(byte[] bytes, Function<ByteBuffer, T> func) {
        List<T> parsedData = new ArrayList<>();
        ByteBuffer byteBuffer = getByteBuffer(bytes);
        while (byteBuffer.remaining() > 0) {
            T item = func.apply(byteBuffer);
            parsedData.add(item);
        }
        return parsedData;
    }

    public static List<Byte> getBytesAsList(byte[] bytes) {
        return getTypeFromBytes(bytes, new Function<ByteBuffer, Byte>() {
            @Override
            public Byte apply(ByteBuffer byteBuffer) {
                return byteBuffer.get();
            }
        });
    }

    public static List<Long> getUnsignedIntsFromBytes(byte[] bytes) {
        return getTypeFromBytes(bytes, new Function<ByteBuffer, Long>() {
            @Override
            public Long apply(ByteBuffer byteBuffer) {
                int signedItem = byteBuffer.getInt();
                long unsignedItem = Integer.toUnsignedLong(signedItem);
                return unsignedItem;
            }
        });
    }

    public static List<Integer> getSignedIntsFromBytes(byte[] bytes) {
        return getTypeFromBytes(bytes, new Function<ByteBuffer, Integer>() {
            @Override
            public Integer apply(ByteBuffer byteBuffer) {
                return byteBuffer.getInt();
            }
        });
    }

    public static List<Float> getFloatsFromBytes(byte[] bytes) {
        return getTypeFromBytes(bytes, new Function<ByteBuffer, Float>() {
            @Override
            public Float apply(ByteBuffer byteBuffer) {
                return byteBuffer.getFloat();
            }
        });
    }

    public static List<Double> getDoublesFromBytes(byte[] bytes) {
        return getTypeFromBytes(bytes, new Function<ByteBuffer, Double>() {
            @Override
            public Double apply(ByteBuffer byteBuffer) {
                return byteBuffer.getDouble();
            }
        });
    }

    public static List<String> getStringsFromBytes(byte[] bytes) {
        // TOOD: This one's harder
        return null;
    }
}
