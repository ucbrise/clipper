package clipper.container.app;

import java.util.Map;

enum HeartbeatType implements ClipperEnum {
  KeepAlive(0),
  RequestContainerMetadata(1);

  private static final String enumName = "heartbeat message type";
  private final int code;

  HeartbeatType(int code) {
    this.code = code;
  }

  @Override
  public int getCode() {
    return code;
  }

  private static final Map<Integer, HeartbeatType> typeResolutionMap =
      EnumUtil.getTypeResolutionMap(HeartbeatType.values());

  public static HeartbeatType fromCode(int code) throws IllegalArgumentException {
    return EnumUtil.getEnumFromCodeOrThrow(code, enumName, typeResolutionMap);
  }
}
