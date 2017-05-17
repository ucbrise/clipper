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
