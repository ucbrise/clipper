package ai.clipper.container.util;

import java.util.HashMap;
import java.util.Map;

public class EnumUtil {
  public static <T extends ClipperEnum> Map<Integer, T> getTypeResolutionMap(T[] values) {
    Map<Integer, T> resolutionMap = new HashMap<>();
    for (T value : values) {
      resolutionMap.put(value.getCode(), value);
    }
    return resolutionMap;
  }

  public static <T extends ClipperEnum> T getEnumFromCodeOrThrow(int code, String enumName,
      Map<Integer, T> typeResolutionMap) throws IllegalArgumentException {
    if (!typeResolutionMap.containsKey(code)) {
      throw new IllegalArgumentException(
          String.format("Attempted to get %s from invalid code \"%d\"", enumName, code));
    }
    return typeResolutionMap.get(code);
  }
}
