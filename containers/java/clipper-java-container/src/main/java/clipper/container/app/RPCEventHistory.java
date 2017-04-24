package clipper.container.app;

import java.util.ArrayList;
import java.util.List;

class RPCEventHistory {

    private final List<RPCEvent> events;
    private final int maxSize;
    private int currIndex = 0;

    RPCEventHistory(int maxSize) {
        if(maxSize <= 0) {
            throw new IllegalArgumentException("Event history must have a positive size!");
        }
        this.maxSize = maxSize;
        events = new ArrayList<>();
    }

    void insert(RPCEvent event) {
        if(events.size() < maxSize) {
            events.add(event);
        } else {
            events.set(currIndex, event);
        }
        currIndex = (currIndex + 1) % maxSize;
    }

    RPCEvent[] getEvents() {
        RPCEvent[] orderedEvents = new RPCEvent[events.size()];
        int startIndex = currIndex % events.size();
        for(int i = 0; i < events.size(); i++) {
            orderedEvents[i] = events.get(startIndex);
            startIndex = (startIndex + 1) % events.size();
        }
        return orderedEvents;
    }

}
