package ai.clipper.rpc;

import java.util.Map;
import ai.clipper.container.util.ClipperEnum;
import ai.clipper.container.util.EnumUtil;

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
