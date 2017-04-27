package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DoubleVector;
import clipper.container.app.data.FloatVector;
import clipper.container.app.data.SerializableString;
import clipper.container.app.logging.RPCEvent;

import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

class RPCTestModel extends Model<DoubleVector> {
  private final ModelContainer<DoubleVector> container;

  RPCTestModel(ModelContainer<DoubleVector> container) {
    super("rpctest_java", 1, DataType.Doubles);
    this.container = container;
  }

  @Override
  public SerializableString predict(DoubleVector inputVector) {
    double clipperTimestamp = inputVector.getData().get();
    RPCEvent[] eventHistory = container.getEventHistory();
    StringBuilder eventCodeJson = new StringBuilder("[");
    boolean addedEvent = false;
    for (int i = 0; i < eventHistory.length; ++i) {
      RPCEvent currEvent = eventHistory[i];
      if (currEvent.getTimestamp() >= clipperTimestamp) {
        if (i > 0 && !addedEvent) {
          // Capture the heartbeat message
          // sent before Clipper came online
          eventCodeJson.append(String.valueOf(eventHistory[i - 1].getEventType().getCode()));
          eventCodeJson.append(", ");
        }
        eventCodeJson.append(currEvent.getEventType().getCode());
        eventCodeJson.append(", ");
        if(!addedEvent) {
          addedEvent = true;
        }
      }
    }
    // Remove trailing comma forom JSO
    int jsonLength = eventCodeJson.length();
    eventCodeJson.delete(jsonLength - 2, jsonLength);

    eventCodeJson.append("]");
    return new SerializableString(eventCodeJson.toString());
  }
}
