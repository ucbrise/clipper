package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.Function;

public class DataUtils {
  private static ByteBuffer getByteBuffer(byte... bytes) {
    ByteBuffer buffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
    return buffer;
  }

  public static byte[] getBytesFromInts(int... data) {
    ByteBuffer byteBuffer = ByteBuffer.allocate(data.length * 4);
    byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
    for (int item : data) {
      byteBuffer.putInt(item);
    }
    return byteBuffer.array();
  }

  public static byte[] getBytesFromLongs(long... data) {
    ByteBuffer byteBuffer = ByteBuffer.allocate(data.length * 8);
    byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
    for (long item : data) {
      byteBuffer.putLong(item);
    }
    return byteBuffer.array();
  }

  public static byte[] getBytesFromFloats(float... data) {
    ByteBuffer byteBuffer = ByteBuffer.allocate(data.length * 4);
    byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
    for (float item : data) {
      byteBuffer.putFloat(item);
    }
    return byteBuffer.array();
  }

  public static byte[] getBytesFromDoubles(double... data) {
    ByteBuffer byteBuffer = ByteBuffer.allocate(data.length * 8);
    byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
    for (double item : data) {
      byteBuffer.putDouble(item);
    }
    return byteBuffer.array();
  }

  private static <T> List<T> getTypeFromBytes(byte[] bytes, Function<ByteBuffer, T> func) {
    List<T> parsedData = new ArrayList<>();
    ByteBuffer byteBuffer = getByteBuffer(bytes);
    while (byteBuffer.remaining() > 0) {
      T item = func.apply(byteBuffer);
      parsedData.add(item);
    }
    return parsedData;
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

  public static List<Long> getLongsFromBytes(byte[] bytes) {
    return getTypeFromBytes(bytes, new Function<ByteBuffer, Long>() {
      @Override
      public Long apply(ByteBuffer byteBuffer) {
        return byteBuffer.getLong();
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
}
