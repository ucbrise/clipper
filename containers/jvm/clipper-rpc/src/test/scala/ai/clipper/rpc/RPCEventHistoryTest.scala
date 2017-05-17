package ai.clipper.rpc

import java.util.ArrayList

import ai.clipper.rpc.logging.{RPCEventHistory, RPCEventType}
import org.scalatest.FunSuite




class RPCEventHistorySuite extends FunSuite {

  test("short event history is correct") {
    val eventHistory = new RPCEventHistory(10)
    val correctEventTypes = new Array[RPCEventType](5)
    for (i <- 1 to 5)  {
      val eventType = RPCEventType.fromCode(i)
      eventHistory.insert(eventType)
      correctEventTypes(i - 1) = eventType
    }
    val events = eventHistory.getEvents()
    assert(events.length == correctEventTypes.length)
    for (i <- 0 until correctEventTypes.length) {
      assert(events(i).getEventType == correctEventTypes(i))
    }
  }

  test("long event history is correct") {
    val numIterations = 60
    val correctEventsLength = 12
    val eventHistory = new RPCEventHistory(12)
    val correctEventTypes = new ArrayList[RPCEventType]()
    for (i <- 0 until numIterations) {
      val code = (i % 6) + 1
      val eventType = RPCEventType.fromCode(code)
      eventHistory.insert(eventType)
      if (numIterations - i <= correctEventsLength) {
        correctEventTypes.add(eventType)
      }
    }

    val events = eventHistory.getEvents()
    assert(events.length == correctEventTypes.size())
    for (i <- 0 until correctEventTypes.size()) {
      assert(events(i).getEventType() == correctEventTypes.get(i))
    }
  }

  test("event history timestamps are ascending") {
    val eventHistory = new RPCEventHistory(10)
    for (i <- 0 until 10) {
      val code = (i % 6) + 1
      val eventType = RPCEventType.fromCode(code)
      eventHistory.insert(eventType)
    }
    var prevTimestamp: Long = 0
    for (event <- eventHistory.getEvents()) {
      val currTimestamp = event.getTimestamp()
      assert(prevTimestamp <= currTimestamp);
      prevTimestamp = currTimestamp;
    }
  }
}




//public class RPCEventHistoryTest {
//  @Test
//  public void testEventHistoryCorrectShort() {
//    RPCEventHistory eventHistory = new RPCEventHistory(10);
//    RPCEventType[] correctEventTypes = new RPCEventType[5];
//    for (int i = 1; i < 6; i++) {
//      RPCEventType eventType = RPCEventType.fromCode(i);
//      eventHistory.insert(eventType);
//      correctEventTypes[i - 1] = eventType;
//    }
//    RPCEvent[] events = eventHistory.getEvents();
//    Assert.assertEquals(events.length, correctEventTypes.length);
//    for (int i = 0; i < correctEventTypes.length; i++) {
//      Assert.assertEquals(events[i].getEventType(), correctEventTypes[i]);
//    }
//  }
//
//  @Test
//  public void testEventHistoryCorrectLong() {
//    int numIterations = 60;
//    int correctEventsLength = 12;
//    RPCEventHistory eventHistory = new RPCEventHistory(12);
//    List<RPCEventType> correctEventTypes = new ArrayList<>();
//    for (int i = 0; i < numIterations; i++) {
//      int code = (i % 6) + 1;
//      RPCEventType eventType = RPCEventType.fromCode(code);
//      eventHistory.insert(eventType);
//      if (numIterations - i <= correctEventsLength) {
//        correctEventTypes.add(eventType);
//      }
//    }
//    RPCEvent[] events = eventHistory.getEvents();
//    Assert.assertEquals(events.length, correctEventTypes.size());
//    for (int i = 0; i < correctEventTypes.size(); i++) {
//      Assert.assertEquals(events[i].getEventType(), correctEventTypes.get(i));
//    }
//  }
//
//  @Test
//  public void testEventHistoryTimestampsAreAscending() {
//    RPCEventHistory eventHistory = new RPCEventHistory(10);
//    for (int i = 0; i < 10; i++) {
//      int code = (i % 6) + 1;
//      RPCEventType eventType = RPCEventType.fromCode(code);
//      eventHistory.insert(eventType);
//    }
//    long prevTimestamp = 0;
//    for (RPCEvent event : eventHistory.getEvents()) {
//      long currTimestamp = event.getTimestamp();
//      Assert.assertTrue(prevTimestamp <= currTimestamp);
//      prevTimestamp = currTimestamp;
//    }
//  }
//}
