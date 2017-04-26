package clipper.container.app;

import clipper.container.app.logging.RPCEvent;
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
    RPCEventType[] correctEventTypes = new RPCEventType[5];
    for (int i = 1; i < 6; i++) {
      RPCEventType eventType = RPCEventType.fromCode(i);
      eventHistory.insert(eventType);
      correctEventTypes[i - 1] = eventType;
    }
    RPCEvent[] events = eventHistory.getEvents();
    Assert.assertEquals(events.length, correctEventTypes.length);
    for (int i = 0; i < correctEventTypes.length; i++) {
      Assert.assertEquals(events[i].getEventType(), correctEventTypes[i]);
    }
  }

  @Test
  public void testEventHistoryCorrectLong() {
    int numIterations = 60;
    int correctEventsLength = 12;
    RPCEventHistory eventHistory = new RPCEventHistory(12);
    List<RPCEventType> correctEventTypes = new ArrayList<>();
    for (int i = 0; i < numIterations; i++) {
      int code = (i % 6) + 1;
      RPCEventType eventType = RPCEventType.fromCode(code);
      eventHistory.insert(eventType);
      if (numIterations - i <= correctEventsLength) {
        correctEventTypes.add(eventType);
      }
    }
    RPCEvent[] events = eventHistory.getEvents();
    Assert.assertEquals(events.length, correctEventTypes.size());
    for (int i = 0; i < correctEventTypes.size(); i++) {
      Assert.assertEquals(events[i].getEventType(), correctEventTypes.get(i));
    }
  }

  @Test
  public void testEventHistoryTimestampsAreAscending() {
    RPCEventHistory eventHistory = new RPCEventHistory(10);
    for (int i = 0; i < 10; i++) {
      int code = (i % 6) + 1;
      RPCEventType eventType = RPCEventType.fromCode(code);
      eventHistory.insert(eventType);
    }
    long prevTimestamp = 0;
    for (RPCEvent event : eventHistory.getEvents()) {
      long currTimestamp = event.getTimestamp();
      Assert.assertTrue(prevTimestamp <= currTimestamp);
      prevTimestamp = currTimestamp;
    }
  }
}
