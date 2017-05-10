package ai.clipper.rpc;

import java.util.Map;
import ai.clipper.container.util.ClipperEnum;
import ai.clipper.container.util.EnumUtil;

enum ContainerMessageType implements ClipperEnum {
  NewContainer(0),
  ContainerContent(1),
  Heartbeat(2);

  private static final String enumName = "container message type";
  private final int code;

  ContainerMessageType(int code) {
    this.code = code;
  }

  @Override
  public int getCode() {
    return code;
  }

  private static final Map<Integer, ContainerMessageType> typeResolutionMap =
      EnumUtil.getTypeResolutionMap(ContainerMessageType.values());

  public static ContainerMessageType fromCode(int code) throws IllegalArgumentException {
    return EnumUtil.getEnumFromCodeOrThrow(code, enumName, typeResolutionMap);
  }
}