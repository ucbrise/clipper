package clipper.container.app.logging;

import clipper.container.app.ClipperEnum;
import clipper.container.app.EnumUtil;

import java.util.Map;

public enum RPCEventType implements ClipperEnum {
    SentHeartbeat(1),
    ReceivedHeartbeat(2),
    SentContainerMetadata(3),
    ReceivedContainerMetadata(4),
    SentContainerContent(5),
    ReceivedContainerContent(6);

    private final int code;

    RPCEventType(int code) {
        this.code = code;
    }

    @Override
    public int getCode() {
        return code;
    }

    private static final Map<Integer, RPCEventType> typeResolutionMap =
            EnumUtil.getTypeResolutionMap(RPCEventType.values());

    public static RPCEventType fromCode(int code) throws IllegalArgumentException {
        return EnumUtil.getEnumFromCodeOrThrow(code, "rpc event", typeResolutionMap);
    }
}
