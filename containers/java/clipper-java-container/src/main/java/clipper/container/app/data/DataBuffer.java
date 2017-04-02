package clipper.container.app.data;

import java.nio.ByteBuffer;

public abstract class DataBuffer<T> {
  abstract void init(ByteBuffer inputBuffer);

  abstract T get(int size);

  abstract T getAll();
}
