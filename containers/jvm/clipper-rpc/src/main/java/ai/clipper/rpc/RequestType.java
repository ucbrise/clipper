package ai.clipper.rpc;

import java.util.HashMap;
import java.util.Map;

public enum RequestType {
  Predict(0),
  Feedback(1);

  private int code;

  RequestType(int code) {
    this.code = code;
  }

  public int getCode() {
    return code;
  }

  private static final Map<Integer, RequestType> typeResolutionMap =
      new HashMap<Integer, RequestType>();
  static {
    for (RequestType type : RequestType.values()) {
      typeResolutionMap.put(type.getCode(), type);
    }
  }

  public static RequestType fromCode(int code) throws IllegalArgumentException {
    if (!typeResolutionMap.containsKey(code)) {
      throw new IllegalArgumentException(
          String.format("Attempted to get request type from invalid code \"%d\"", code));
    }
    return typeResolutionMap.get(code);
  }
}
