package ai.clipper.container.data;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Iterator;

public abstract class DataVectorParser<U, T extends DataVector<U>> {
  public abstract T constructDataVector(ByteBuffer data, long byteSize);
}
