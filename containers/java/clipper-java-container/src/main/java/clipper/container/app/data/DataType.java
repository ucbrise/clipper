package clipper.container.app.data;

import java.util.HashMap;
import java.util.Map;

public enum DataType {
  Bytes(0, "bytes"),
  Ints(1, "ints"),
  Floats(2, "floats"),
  Doubles(3, "doubles"),
  Strings(4, "strings");

  private int code;
  private String name;

  DataType(int code, String name) {
    this.code = code;
    this.name = name;
  }

  public int getCode() {
    return code;
  }

  public String toString() {
    return name;
  }

  private static final Map<Integer, DataType> typeResolutionMap = new HashMap<Integer, DataType>();
  static {
    for (DataType type : DataType.values()) {
      typeResolutionMap.put(type.getCode(), type);
    }
  }

  public static DataType fromCode(int code) throws IllegalArgumentException {
    if (!typeResolutionMap.containsKey(code)) {
      throw new IllegalArgumentException(
          String.format("Attempted to get data type from invalid code \"%d\"", code));
    }
    return typeResolutionMap.get(code);
  }
}
