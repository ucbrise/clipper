package ai.clipper.container.data;

import sun.nio.cs.US_ASCII;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.IntBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.CoderResult;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;

public class SerializableString extends DataVector<String> {
  private static int MAXIMUM_UTF_8_CHAR_LENGTH_BYTES = 4;

  public SerializableString(String data) {
    super(data);
  }

  /**
   * Writes the byte representation of the UTF-8
   * encoded string to the provided buffer
   *
   * @return The number of bytes written to the buffer
   */
  public int encodeUTF8ToBuffer(ByteBuffer buffer) {
    CharBuffer stringContent = CharBuffer.wrap(data);
    Charset UTFCharset = StandardCharsets.UTF_8;
    CharsetEncoder encoder = UTFCharset.newEncoder();
    int startPosition = buffer.position();
    encoder.encode(stringContent, buffer, true);
    int endPosition = buffer.position();
    return (endPosition - startPosition);
  }

  /**
   * @return The maximum size of the utf-8 encoded string
   * in bytes. The actual byte size of a fixed-length string
   * may vary depending on its character content.
   */
  public int maxSizeBytes() {
    return data.length() * MAXIMUM_UTF_8_CHAR_LENGTH_BYTES;
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
          while (iterBuffer.get() != '\0') {
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
