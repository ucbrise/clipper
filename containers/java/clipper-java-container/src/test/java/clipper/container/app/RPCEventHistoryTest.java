package clipper.container.app;

import clipper.container.app.logging.RPCEventHistory;
import clipper.container.app.logging.RPCEventType;
import org.junit.Assert;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

public class RPCEventHistoryTest {

    @Test
    public void testEventHistoryCorrectShort() {
        RPCEventHistory eventHistory = new RPCEventHistory(10);
        RPCEventType[] correctEvents = new RPCEventType[5];
        for(int i = 1; i < 6; i++) {
            RPCEventType event = RPCEventType.fromCode(i);
            eventHistory.insert(RPCEventType.fromCode(i));
            correctEvents[i - 1] = event;
        }
        Assert.assertArrayEquals(eventHistory.getEvents(), correctEvents);
    }

    @Test
    public void testEventHistoryCorrectLong() {
        int numIterations = 60;
        int correctEventsLength = 12;
        RPCEventHistory eventHistory = new RPCEventHistory(12);
        List<RPCEventType> correctEvents = new ArrayList<>();
        for(int i = 0; i < numIterations; i++) {
            int code = (i % 6) + 1;
            RPCEventType event = RPCEventType.fromCode(code);
            eventHistory.insert(event);
            if(numIterations - i <= correctEventsLength) {
                correctEvents.add(event);
            }
        }
        Assert.assertArrayEquals(eventHistory.getEvents(), correctEvents.toArray());
    }
}
