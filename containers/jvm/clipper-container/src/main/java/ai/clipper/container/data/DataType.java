package ai.clipper.container.data;

import ai.clipper.container.util.ClipperEnum;
import ai.clipper.container.util.EnumUtil;

import java.util.Map;

public enum DataType implements ClipperEnum {
  Bytes(0, "bytes"),
  Ints(1, "ints"),
  Floats(2, "floats"),
  Doubles(3, "doubles"),
  Strings(4, "strings");

  private static final String enumName = "data type";
  private final int code;
  private final String name;

  DataType(int code, String name) {
    this.code = code;
    this.name = name;
  }

  @Override
  public int getCode() {
    return code;
  }

  public String toString() {
    return name;
  }

  private static final Map<Integer, DataType> typeResolutionMap =
      EnumUtil.getTypeResolutionMap(DataType.values());

  public static DataType fromCode(int code) throws IllegalArgumentException {
    return EnumUtil.getEnumFromCodeOrThrow(code, enumName, typeResolutionMap);
  }
}
