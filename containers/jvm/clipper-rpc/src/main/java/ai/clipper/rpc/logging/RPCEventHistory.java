package ai.clipper.rpc.logging;

import java.util.ArrayList;
import java.util.List;

/**
 * A circular buffer containing the most
 * recent RPC events noted by the container.
 * See {@link RPCEventType} for the different
 * event types
 */
public class RPCEventHistory {
  private final List<RPCEvent> events;
  private final int maxSize;
  private int currIndex = 0;

  /**
   * @param maxSize Specifies the size of the
   * circular buffer. Only maxSize events can
   * be stored at one time. When at capacity,
   * the oldest events are replaced by new ones
   * upon calls to insert
   */
  public RPCEventHistory(int maxSize) {
    if (maxSize <= 0) {
      throw new IllegalArgumentException("Event history must have a positive size!");
    }
    this.maxSize = maxSize;
    events = new ArrayList<>();
  }

  public void insert(RPCEventType eventType) {
    RPCEvent event = new RPCEvent(System.currentTimeMillis(), eventType);
    if (events.size() < maxSize) {
      events.add(event);
    } else {
      events.set(currIndex, event);
    }
    currIndex = (currIndex + 1) % maxSize;
  }

  public RPCEvent[] getEvents() {
    RPCEvent[] orderedEvents = new RPCEvent[events.size()];
    int index = currIndex % events.size();
    for (int i = 0; i < events.size(); i++) {
      orderedEvents[i] = events.get(index);
      index = (index + 1) % events.size();
    }
    return orderedEvents;
  }
}
