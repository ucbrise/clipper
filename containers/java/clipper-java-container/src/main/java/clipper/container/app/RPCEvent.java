package clipper.container.app;

import java.util.Map;

public enum RPCEvent implements ClipperEnum {
    SentHeartbeat(1),
    ReceivedHeartbeat(2),
    SentContainerMetadata(3),
    ReceivedContainerMetadata(4),
    SentContainerContent(5),
    ReceivedContainerContent(6);

    private final int code;

    RPCEvent(int code) {
        this.code = code;
    }

    @Override
    public int getCode() {
        return code;
    }

    private static final Map<Integer, RPCEvent> typeResolutionMap =
            EnumUtil.getTypeResolutionMap(RPCEvent.values());

    public static RPCEvent fromCode(int code) throws IllegalArgumentException {
        return EnumUtil.getEnumFromCodeOrThrow(code, "rpc event", typeResolutionMap);
    }
}
