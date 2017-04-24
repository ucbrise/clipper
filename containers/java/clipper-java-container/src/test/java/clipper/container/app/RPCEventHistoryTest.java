package clipper.container.app;

import org.junit.Assert;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

public class RPCEventHistoryTest {

    @Test
    public void testEventHistoryCorrectShort() {
        RPCEventHistory eventHistory = new RPCEventHistory(10);
        RPCEvent[] correctEvents = new RPCEvent[5];
        for(int i = 1; i < 6; i++) {
            RPCEvent event = RPCEvent.fromCode(i);
            eventHistory.insert(RPCEvent.fromCode(i));
            correctEvents[i - 1] = event;
        }
        Assert.assertArrayEquals(eventHistory.getEvents(), correctEvents);
    }

    @Test
    public void testEventHistoryCorrectLong() {
        int numIterations = 60;
        int correctEventsLength = 12;
        RPCEventHistory eventHistory = new RPCEventHistory(12);
        List<RPCEvent> correctEvents = new ArrayList<>();
        for(int i = 0; i < numIterations; i++) {
            int code = (i % 6) + 1;
            RPCEvent event = RPCEvent.fromCode(code);
            eventHistory.insert(event);
            if(numIterations - i <= correctEventsLength) {
                correctEvents.add(event);
            }
        }
        Assert.assertArrayEquals(eventHistory.getEvents(), correctEvents.toArray());
    }
}
