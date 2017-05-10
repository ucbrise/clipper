package ai.clipper.rpc.logging;

public class RPCEvent {
  private final long timestamp;
  private final RPCEventType eventType;

  public RPCEvent(long timestamp, RPCEventType eventType) {
    this.timestamp = timestamp;
    this.eventType = eventType;
  }

  public long getTimestamp() {
    return timestamp;
  }

  public RPCEventType getEventType() {
    return eventType;
  }
}
