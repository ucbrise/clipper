package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DoubleVector;
import clipper.container.app.data.FloatVector;
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
  public FloatVector predict(DoubleVector inputVector) {
    double clipperTimestamp = inputVector.getData().get();
    RPCEvent[] eventHistory = container.getEventHistory();
    List<Float> eventCodes = new ArrayList<>();
    for (int i = 0; i < eventHistory.length; i++) {
      RPCEvent currEvent = eventHistory[i];
      if (currEvent.getTimestamp() >= clipperTimestamp) {
        if (i > 0 && eventCodes.size() == 0) {
          // Capture the heartbeat message
          // sent before Clipper came online
          eventCodes.add((float) eventHistory[i - 1].getEventType().getCode());
        }
        eventCodes.add((float) currEvent.getEventType().getCode());
      }
    }
    FloatBuffer buffer = FloatBuffer.allocate(eventCodes.size());
    for (float code : eventCodes) {
      buffer.put(code);
    }
    buffer.rewind();
    return new FloatVector(buffer);
  }
}
