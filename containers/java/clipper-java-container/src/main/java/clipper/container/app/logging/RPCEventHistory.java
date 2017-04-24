package clipper.container.app.logging;

import java.util.ArrayList;
import java.util.List;

public class RPCEventHistory {

    private final List<RPCEvent> events;
    private final int maxSize;
    private int currIndex = 0;

    public RPCEventHistory(int maxSize) {
        if(maxSize <= 0) {
            throw new IllegalArgumentException("Event history must have a positive size!");
        }
        this.maxSize = maxSize;
        events = new ArrayList<>();
    }

    public void insert(RPCEventType eventType) {
        RPCEvent event = new RPCEvent(System.currentTimeMillis(), eventType);
        if(events.size() < maxSize) {
            events.add(event);
        } else {
            events.set(currIndex, event);
        }
        currIndex = (currIndex + 1) % maxSize;
    }


    public RPCEvent[] getEvents() {
        RPCEvent[] orderedEvents = new RPCEvent[events.size()];
        int index = currIndex % events.size();
        for(int i = 0; i < events.size(); i++) {
            orderedEvents[i] = events.get(index);
            index = (index + 1) % events.size();
        }
        return orderedEvents;
    }
}
