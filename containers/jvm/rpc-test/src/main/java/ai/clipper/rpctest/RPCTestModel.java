package ai.clipper.rpctest;

import ai.clipper.container.ClipperModel;

import ai.clipper.container.data.DataType;
import ai.clipper.container.data.DoubleVector;
import ai.clipper.container.data.SerializableString;
import ai.clipper.rpc.RPC;
import ai.clipper.rpc.logging.RPCEvent;

import java.util.ArrayList;

class RPCTestModel extends ClipperModel<DoubleVector> {
  private final RPC<DoubleVector> rpc;

  RPCTestModel(RPC<DoubleVector> rpc) {
    this.rpc = rpc;
  }

  @Override
  public ArrayList<SerializableString> predict(ArrayList<DoubleVector> inputs) {
    ArrayList<SerializableString> outputs = new ArrayList<>();
    for (DoubleVector inputVector : inputs) {
      double clipperTimestamp = inputVector.getData().get();
      RPCEvent[] eventHistory = rpc.getEventHistory();
      // Begin building a JSON array
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
          if (!addedEvent) {
            addedEvent = true;
          }
        }
      }
      // Remove trailing comma from JSON and close
      // the array
      int jsonLength = eventCodeJson.length();
      eventCodeJson.delete(jsonLength - 2, jsonLength);
      eventCodeJson.append("]");

      outputs.add(new SerializableString(eventCodeJson.toString()));
    }
    return outputs;
  }

  @Override
  public DataType getInputType() {
    return DataType.Doubles;
  }
}
