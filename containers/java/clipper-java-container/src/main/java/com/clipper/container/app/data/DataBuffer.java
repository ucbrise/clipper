package com.clipper.container.app.data;

import com.clipper.container.app.Pair;

import java.nio.ByteBuffer;

public abstract class DataBuffer<T> {

  static int INITIAL_BUFFER_SIZE = 1000;

  abstract void init(ByteBuffer buffer);

  abstract T get(int offset, int size);

  abstract T getAll();
}
