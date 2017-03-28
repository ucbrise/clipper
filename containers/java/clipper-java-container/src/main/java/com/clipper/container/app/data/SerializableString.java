package com.clipper.container.app.data;

import sun.nio.cs.US_ASCII;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.IntBuffer;
import java.nio.charset.Charset;
import java.util.Iterator;

public class SerializableString extends DataVector<String> {

  public SerializableString(String data) {
    super(data);
  }

  public static class Parser extends DataVectorParser<String, SerializableString> {
    @Override
    SerializableString constructDataVector(String data) {
      return null;
    }

    @Override
    DataBuffer<String> createDataBuffer() {
      return null;
    }

    @Override
    public Iterator<SerializableString> parseDataVectors(ByteBuffer byteBuffer, IntBuffer splits) {
      byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
      Charset asciiCharSet = US_ASCII.defaultCharset();
      CharBuffer iterBuffer = asciiCharSet.decode(byteBuffer);
      CharBuffer copyBuffer = iterBuffer.duplicate();

      return new Iterator<SerializableString>() {

        @Override
        public boolean hasNext() {
          return iterBuffer.hasRemaining();
        }

        @Override
        public SerializableString next() {
          int stringLength = 0;
          while(iterBuffer.get() != '\0') {
            stringLength++;
          }
          char[] stringChars = new char[stringLength];
          copyBuffer.get(stringChars, 0, stringLength);
          copyBuffer.get();
          String stringData = new String(stringChars);
          return new SerializableString(stringData);
        }
      };
    }
  }
}
